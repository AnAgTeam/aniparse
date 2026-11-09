#include "TestRequestContext.hpp"
#include "catch_amalgamated.hpp"
#include <aniparse/source/Parser.hpp>
using namespace aniparse;
using FakeCookieJar = MemoryCookieJar;

namespace {
struct CredentialProfile : ParserProfile {
    using ParserProfile::ParserProfile;
    AuthKeys auth_keys() const noexcept override { return {{"session"}, {"Authorization"}, {"token"}}; }
};
SessionState saved_credentials() {
    SessionState saved{.parser_id = "Test", .profile_id = "main"};
    saved.auth.cookies = {{.name = "session", .value = "secret", .domain = "example.com"},
                          {.name = "antibot", .value = "volatile", .domain = "example.com"}};
    saved.auth.headers = {{"Authorization", "Bearer xyz"}, {"User-Agent", "volatile-agent"}};
    saved.auth.url_params = {{"token", "kept"}, {"ts", "dropped"}};
    saved.settings.base_url = "https://selected.example";
    return saved;
}
}

TEST_CASE("Session export keeps only declared durable credentials and user choices") {
    auto profile = test_profile<CredentialProfile>();
    auto session = profile->restore_session(test_services(), saved_credentials());
    REQUIRE(session);
    auto saved = session->export_state();
    REQUIRE(saved.auth.cookies.size() == 1);
    CHECK(saved.auth.cookies.front().name == "session");
    REQUIRE(saved.auth.headers.size() == 1);
    CHECK(saved.auth.headers.at("Authorization") == "Bearer xyz");
    REQUIRE(saved.auth.url_params.size() == 1);
    CHECK(saved.auth.url_params.at("token") == "kept");
    CHECK(saved.settings.base_url == "https://selected.example");
    auto restored = profile->restore_session(test_services(), saved);
    REQUIRE(restored);
    CHECK(restored->config()->cookie_jar != session->config()->cookie_jar);
    CHECK_FALSE(restored->config()->cookie_jar->find_cookie("antibot"));
    CHECK_FALSE(restored->config()->headers.contains("User-Agent"));
    CHECK_FALSE(restored->config()->url_params.contains("ts"));
}

TEST_CASE("A profile without declared credentials exports only user choices") {
    auto profile = test_profile();
    auto session = profile->restore_session(test_services(), saved_credentials());
    REQUIRE(session);
    auto saved = session->export_state();
    CHECK(saved.auth.cookies.empty());
    CHECK(saved.auth.headers.empty());
    CHECK(saved.auth.url_params.empty());
    CHECK(saved.settings.base_url == "https://selected.example");
}

#include "CoroTest.hpp"

namespace {
class WebLoginClient final : public HttpClient {
public:
    RequestTask<HttpResponse> do_request(ConfiguredGetRequest) override { co_return HttpResponse{}; }
    RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override { co_return HttpResponse{}; }
    RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override { co_return HttpResponse{}; }
    void apply_client_config(HttpClientConfig) override {}
    std::shared_ptr<CookieJar> make_cookie_jar() override { return std::make_shared<FakeCookieJar>(); }
};

class StorageProfile final : public ParserProfile {
public:
    using ParserProfile::ParserProfile;
    mutable int exchanges = 0;
    std::optional<AuthInfo> auth_info() const override {
        AuthInfo info;
        info.method = AuthMethod::InteractiveWeb;
        auto& web = info.web_login.emplace();
        web.material = WebLoginInfo::Material::StorageToken;
        web.login_url = "https://example.com/login";
        web.storage_key = "auth";
        web.storage_token_path = "token.access_token";
        return info;
    }
    RequestTask<void> authenticate_impl(
        AuthenticationSession& candidate, AuthenticationData credentials) const override {
        ++exchanges;
        const auto& token = std::get<AuthenticationToken>(credentials).token;
        if (token == "rejected") {
            co_return make_request_error(RequestErrorCode::InvalidCredentials, "Rejected");
        }
        if (token == "empty") co_return make_request_error(RequestErrorCode::UnexpectedResponse, "Empty token");
        auto* config = &candidate.config();
        config->headers.set("Authorization", "Bearer " + token);
        co_return {};
    }
};
}

CORO_TEST_CASE("Web login verifies storage tokens only at the declared origin", "[auth][web-login]") {
    auto owned_profile = test_profile<StorageProfile>("example");
    auto& profile = *owned_profile;
    auto context = profile.begin_web_login(profile.make_session(test_services(std::make_shared<WebLoginClient>())));
    auto observations = profile.web_login_observation(context);
    REQUIRE(observations.storage.size() == 1);
    CHECK(observations.storage.front().origin == "https://example.com");
    WebLoginState state;
    auto pending = co_await profile.check_web_login(context, WebLoginEventKind::Started, state);
    REQUIRE(pending);
    CHECK(std::holds_alternative<InteractionPending>(*pending));
    state.storage.push_back({{"https://oauth.example.com", WebStorageKind::Local, "auth"},
                             R"({"token":{"access_token":"valid"}})"});
    pending = co_await profile.check_web_login(context, WebLoginEventKind::StorageChanged, state);
    REQUIRE(pending);
    CHECK(std::holds_alternative<InteractionPending>(*pending));
    CHECK(profile.exchanges == 0);
    state.storage.front().location.origin = "https://example.com";
    state.storage.front().location.storage = WebStorageKind::Session;
    pending = co_await profile.check_web_login(context, WebLoginEventKind::StorageChanged, state);
    REQUIRE(pending);
    CHECK(std::holds_alternative<InteractionPending>(*pending));
    state.storage.front().location.storage = WebStorageKind::Local;
    for (const auto& value : {"broken-json", R"({"token":{}})", R"({"token":{"access_token":false}})"}) {
        state.storage.front().value = value;
        pending = co_await profile.check_web_login(context, WebLoginEventKind::TimerElapsed, state);
        REQUIRE(pending);
        CHECK(std::holds_alternative<InteractionPending>(*pending));
    }
    CHECK(profile.exchanges == 0);
    state.storage.front().value = R"({"token":{"access_token":"valid"}})";
    auto verified = co_await profile.check_web_login(context, WebLoginEventKind::StorageChanged, state);
    REQUIRE(verified);
    const auto* completed = std::get_if<InteractionCompleted<ProfileSession>>(&*verified);
    REQUIRE(completed);
    CHECK(completed->value.config()->headers.at("Authorization") == "Bearer valid");
    CHECK_FALSE(context.config()->headers.contains("Authorization"));
    CHECK(completed->value.config()->cookie_jar != context.config()->cookie_jar);
    state.storage.front().value = R"({"token":{"access_token":"rejected"}})";
    auto rejected = co_await profile.check_web_login(context, WebLoginEventKind::ManualCheckRequested, state);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code == RequestErrorCode::InvalidCredentials);
    CHECK_FALSE(context.config()->headers.contains("Authorization"));
    state.storage.front().value = R"({"token":{"access_token":"empty"}})";
    auto invalid = co_await profile.check_web_login(context, WebLoginEventKind::StorageChanged, state);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().code == RequestErrorCode::UnexpectedResponse);
    state.storage.front().value.reset();
    pending = co_await profile.check_web_login(context, WebLoginEventKind::StorageChanged, state);
    REQUIRE(pending);
    CHECK(std::holds_alternative<InteractionPending>(*pending));
}

TEST_CASE("Web storage origins normalize scheme host and port", "[auth][web-login]") {
    CHECK(web_login_origin("https://EXAMPLE.com:443/path") == "https://example.com");
    CHECK(web_login_origin("https://example.com:8443/path") == "https://example.com:8443");
    CHECK(web_login_origin("https://user:pass@example.com/login") == "https://example.com");
    CHECK(web_login_origin("file:///tmp/token").empty());
    CHECK(web_login_origin("not a URL").empty());
}

TEST_CASE("Browser subscriptions combine flags without changing event identity", "[auth][web-login]") {
    WebLoginObservationConfig config;
    CHECK(config.events == all_web_login_events);
    for (int index = 0; index < static_cast<int>(WebLoginEventKind::Count); ++index) {
        CHECK(config.observes(static_cast<WebLoginEventKind>(index)));
    }
    CHECK_FALSE(config.observes(WebLoginEventKind::Count));
    CHECK(web_login_event_flag(WebLoginEventKind::Count).none());
    CHECK(config.observes(WebLoginEventKind::Started));
    CHECK(config.observes(WebLoginEventKind::ManualCheckRequested));
    config.events = web_login_event_flag(WebLoginEventKind::CookiesChanged) |
                    web_login_event_flag(WebLoginEventKind::StorageChanged);
    CHECK(config.observes(WebLoginEventKind::CookiesChanged));
    CHECK(config.observes(WebLoginEventKind::StorageChanged));
    CHECK_FALSE(config.observes(WebLoginEventKind::TimerElapsed));
    config.events |= web_login_event_flag(WebLoginEventKind::CookiesChanged);
    CHECK(config.events == (web_login_event_flag(WebLoginEventKind::CookiesChanged) |
                            web_login_event_flag(WebLoginEventKind::StorageChanged)));
    config.events &= ~web_login_event_flag(WebLoginEventKind::CookiesChanged);
    CHECK_FALSE(config.observes(WebLoginEventKind::CookiesChanged));
    CHECK(config.observes(WebLoginEventKind::StorageChanged));
    config.events.reset();
    CHECK_FALSE(config.observes(WebLoginEventKind::StorageChanged));
    // Unknown enum values must neither shift beyond the mask nor match an empty flag.
    const auto unknown = static_cast<WebLoginEventKind>(-1);
    CHECK_FALSE(config.observes(unknown));
    CHECK(web_login_event_flag(unknown).none());
    config.events.set();
    CHECK(config.observes(WebLoginEventKind::Started));
    CHECK_FALSE(config.observes(unknown));
}

TEST_CASE("Interaction states support owned payloads and payload-free completion", "[interaction]") {
    InteractionState<std::unique_ptr<int>> state = InteractionPending{};
    REQUIRE(std::holds_alternative<InteractionPending>(state));
    state = InteractionCompleted{std::make_unique<int>(42)};
    auto moved = std::move(state);
    const auto& completed = std::get<InteractionCompleted<std::unique_ptr<int>>>(moved);
    REQUIRE(completed.value);
    CHECK(*completed.value == 42);

    InteractionState<std::monostate> confirmation = InteractionCompleted{std::monostate{}};
    CHECK(std::holds_alternative<InteractionCompleted<std::monostate>>(confirmation));
}

CORO_TEST_CASE("Non-browser authentication has no browser observations", "[auth][web-login]") {
    class PasswordProfile final : public ParserProfile {
    public:
        using ParserProfile::ParserProfile;
        std::optional<AuthInfo> auth_info() const override { return AuthInfo{}; }
    };
    auto owned_profile = test_profile<PasswordProfile>("example", {"password", "Account"});
    auto& profile = *owned_profile;
    REQUIRE_FALSE(profile.auth_info()->web_login);
    const auto context = profile.begin_web_login(profile.make_session(test_services(std::make_shared<WebLoginClient>())));
    const auto observation = profile.web_login_observation(context);
    CHECK(observation.events.none());
    CHECK(observation.storage.empty());
    CHECK(observation.interval_ms == 0);
    const auto checked = co_await profile.check_web_login(context, WebLoginEventKind::Started, {});
    REQUIRE_FALSE(checked);
    CHECK(checked.error().code == RequestErrorCode::NotImplemented);
}

TEST_CASE("Session persistence supports transports without cookie jars") {
    struct Client : HttpClient {
        RequestTask<HttpResponse> do_request(ConfiguredGetRequest) override { co_return HttpResponse{}; }
        RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override { co_return HttpResponse{}; }
        RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override { co_return HttpResponse{}; }
        void apply_client_config(HttpClientConfig) override {}
        std::shared_ptr<CookieJar> make_cookie_jar() override { return nullptr; }
    };
    auto profile = test_profile<CredentialProfile>();
    auto session = profile->restore_session(test_services(std::make_shared<Client>()), saved_credentials());
    REQUIRE(session);
    auto saved = session->export_state();
    CHECK(saved.auth.cookies.empty());
    CHECK(saved.auth.headers.at("Authorization") == "Bearer xyz");
    CHECK(saved.auth.url_params.at("token") == "kept");
}
