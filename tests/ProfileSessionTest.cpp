#include "CoroTest.hpp"
#include <aniparse/catalog/CatalogRootGetter.hpp>
#include <aniparse/source/Parser.hpp>
#include <aniparse/net/CookieStore.hpp>
#include <array>
#include <ranges>

using namespace aniparse;

namespace {
struct SessionClient : HttpClient {
    std::vector<ConfiguredGetRequest> requests;
    std::function<void()> on_request;
    std::optional<RequestErrorCode> failure;
    RequestTask<HttpResponse> do_request(ConfiguredGetRequest request) override {
        requests.push_back(std::move(request));
        if (on_request) on_request();
        if (failure) co_return make_request_error(*failure, "Injected transport result");
        co_return HttpResponse{.status_code = 200};
    }
    RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override { co_return HttpResponse{}; }
    RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override { co_return HttpResponse{}; }
    void apply_client_config(HttpClientConfig) override {}
    std::shared_ptr<CookieJar> make_cookie_jar() override { return std::make_shared<MemoryCookieJar>(); }
};

class SessionProfile : public ParserProfile {
public:
    using ParserProfile::ParserProfile;
    void configure(ParserConfig& config) const override {
        config.headers.set("X-Protocol", "fixed");
        config.modifiers.emplace_back([](HttpRequest& request, const RequestContext& context) {
            std::visit([&](auto& value) {
                value.headers.set("Referer", std::string(context.canonical_base_url()));
            }, request);
        });
    }
    std::span<const std::string_view> mirrors() const override {
        static constexpr std::array<std::string_view, 2> seeds{"https://api.example", "https://backup.example"};
        return seeds;
    }
    std::string canonical_base_url() const override { return "https://web.example"; }
    AuthKeys auth_keys() const noexcept override { return {{"login"}, {"Authorization"}, {"key"}}; }
    std::optional<AuthInfo> auth_info() const override {
        AuthInfo info;
        info.method = AuthMethod::InteractiveWeb;
        auto& web = info.web_login.emplace();
        web.login_url = "https://web.example";
        web.material = WebLoginInfo::Material::StorageToken;
        web.storage_key = "token";
        return info;
    }
    std::optional<AuthInfo> auth_info(RequestContext context) const override {
        auto info = auth_info();
        info->web_login->login_url = context.canonical_base_url();
        return info;
    }
protected:
    RequestTask<void> authenticate_impl(AuthenticationSession& candidate, AuthenticationData data) const override {
        REQUIRE_FALSE(candidate.config().headers.contains("Authorization"));
        REQUIRE_FALSE(candidate.config().url_params.contains("key"));
        REQUIRE(candidate.config().cookie_jar->cookies().empty());
        auto token = std::get<AuthenticationToken>(data).token;
        candidate.config().headers.set("Authorization", token);
        candidate.config().url_params["key"] = token;
        candidate.config().headers.set("X-Transient", "login-state");
        candidate.config().cookie_jar->set_cookie({.name = "login", .value = token, .domain = "api.example"});
        auto& context = candidate.context();
        for (int index = 0; index < 2; ++index) {
            auto response = co_await context.request(GetRequest{
                .url = std::string(context.base_url()) + "/login"});
            if (!response) co_return unexpected(std::move(response.error()));
        }
        if (token == "reject") co_return make_request_error(RequestErrorCode::InvalidCredentials, "Rejected");
        co_return {};
    }
};

struct SessionParser : Parser {
    ParserInfo info() const override { return {.name = "Session test"}; }
    std::string identifier() const override { return "session-test"; }
    void emplace_domains(EmplaceDomainsSink&) const override {}
    std::unique_ptr<ParserProfile> make_profile_impl(ProfileInfo info) const override {
        return std::make_unique<SessionProfile>(std::move(info));
    }
};

auto generation(std::string api, std::string frontend) {
    auto mirrors = std::make_shared<const MirrorSource>(
        std::map<std::string, std::vector<std::string>, std::less<>>{},
        MirrorSource::ProfileMirrors{{{"session-test", "main"}, {std::move(api)}}},
        MirrorSource::ProfileOrigins{{{"session-test", "main"}, std::move(frontend)}});
    return std::make_shared<const OverridesState>(std::move(mirrors));
}

struct Fixture {
    SessionParser parser;
    std::unique_ptr<ParserProfile> profile = parser.make_profile("main");
    std::shared_ptr<SessionClient> client = std::make_shared<SessionClient>();
    std::shared_ptr<RuntimeServices> services = std::make_shared<RuntimeServices>();
    Fixture() { services->client = client; }
};

class SessionCatalog final : public CatalogRootGetter {
protected:
    RequestTask<PageResults<CatalogEntry>> latest_items(RequestContext context, GetFilters) const override {
        auto response = co_await context.request(GetRequest{
            .url = std::string(context.base_url()) + "/latest"});
        if (!response) co_return unexpected(std::move(response.error()));
        co_return PageResults<CatalogEntry>{};
    }
};
}

CORO_TEST_CASE("Implicit sessions capture catalog requests before a deferred task starts", "[session]") {
    Fixture f;
    SessionCatalog catalog;
    HttpClientConfig defaults;
    defaults.request_defaults.headers.set("X-App", "original");
    f.client->set_config(defaults);
    auto session = f.profile->make_session(f.services);
    const auto& current = session;
    auto grouped = session.snapshot();
    auto pending = catalog.latest(current, {});
    CHECK(f.client->requests.empty());

    REQUIRE(session.set_mirror("https://chosen.example"));
    f.services->overrides->store(generation("https://remote.example", "https://new-web.example"));
    defaults.request_defaults.headers.set("X-App", "updated");
    f.client->set_config(std::move(defaults));

    REQUIRE(co_await std::move(pending));
    REQUIRE(co_await catalog.latest(grouped, {}));
    REQUIRE(co_await catalog.latest(current, {}));
    REQUIRE(f.client->requests.size() == 3);
    for (const auto& request : f.client->requests | std::views::take(2)) {
        CHECK(request.request.url == "https://api.example/latest");
        CHECK(request.request.headers.at("Referer") == "https://web.example");
        CHECK(request.request.headers.at("X-App") == "original");
        CHECK(request.cookies == session.config()->cookie_jar);
    }
    const auto& latest = f.client->requests.back();
    CHECK(latest.request.url == "https://chosen.example/latest");
    CHECK(latest.request.headers.at("Referer") == "https://new-web.example");
    CHECK(latest.request.headers.at("X-App") == "updated");

    // A temporary session may disappear before its task is awaited.
    auto temporary = catalog.latest(f.profile->make_session(f.services), {});
    REQUIRE(co_await std::move(temporary));
    CHECK(f.client->requests.back().request.url == "https://remote.example/latest");
    CHECK(f.client->requests.back().cookies != session.config()->cookie_jar);
}

TEST_CASE("Session snapshots retain settings and origins without copying request configuration", "[session]") {
    Fixture f;
    auto session = f.profile->make_session(f.services);
    auto copy = session;
    auto old = session.snapshot();
    REQUIRE(session.set_mirror("https://CUSTOM.example/api/"));
    auto current = copy.snapshot();
    CHECK(current.base_url() == "https://custom.example/api");
    CHECK(old.base_url() == "https://api.example");
    CHECK(current.canonical_base_url() == "https://web.example");
    CHECK(current.config() == old.config());
    auto settings = session.settings();
    settings.canonical_base_url = "https://custom-web.example/";
    REQUIRE(session.set_settings(settings));
    CHECK(session.snapshot().config() == old.config());
    CHECK(session.snapshot().canonical_base_url() == "https://custom-web.example");
    settings.canonical_base_url.reset();
    REQUIRE(session.set_settings(settings));
    f.services->overrides->store(generation("https://new-api.example", "https://new-web.example"));
    CHECK(session.snapshot().base_url() == "https://custom.example/api");
    CHECK(session.snapshot().canonical_base_url() == "https://new-web.example");
    CHECK(current.canonical_base_url() == "https://web.example");
    REQUIRE(session.set_mirror(std::nullopt));
    CHECK(session.snapshot().base_url() == "https://new-api.example");
    CHECK_FALSE(session.set_mirror("https://user:password@example.org"));
    CHECK_FALSE(session.set_mirror("file:///tmp/test"));
    CHECK_FALSE(session.set_mirror("https://example.org/?query=1"));
    CHECK(session.snapshot().base_url() == "https://new-api.example");
    f.profile.reset();
    CHECK(session.snapshot().base_url() == "https://new-api.example");
}

CORO_TEST_CASE("Authentication owns a clean candidate and pins the whole login operation", "[session]") {
    Fixture f;
    auto session = f.profile->make_session(f.services);
    auto first = co_await f.profile->authenticate(session, AuthenticationToken{"first", {}});
    REQUIRE(first);
    CHECK(session.config()->cookie_jar->cookies().empty());
    CHECK(first->config()->cookie_jar != session.config()->cookie_jar);
    auto old = first->snapshot();
    auto task = f.profile->authenticate(*first, AuthenticationToken{"second", {}});
    REQUIRE(first->set_mirror("https://later.example"));
    f.client->requests.clear();
    f.client->on_request = [&] {
        f.services->overrides->store(generation("https://changed.example", "https://changed-web.example"));
    };
    auto second = co_await std::move(task);
    REQUIRE(second);
    REQUIRE(f.client->requests.size() == 2);
    for (const auto& request : f.client->requests) {
        CHECK(request.request.url == "https://api.example/login");
        CHECK(request.request.headers.at("Referer") == "https://web.example");
        CHECK(request.cookies == second->config()->cookie_jar);
    }
    CHECK(old.config()->headers.at("Authorization") == "first");
    CHECK(second->config()->headers.at("Authorization") == "second");
    CHECK(second->snapshot().base_url() == "https://changed.example");
    auto rejected = co_await f.profile->authenticate(*second, AuthenticationToken{"reject", {}});
    REQUIRE_FALSE(rejected);
    CHECK(second->config()->cookie_jar->find_cookie("login")->value == "second");
    CHECK(second->config()->headers.at("Authorization") == "second");
}

CORO_TEST_CASE("Session persistence restores defaults and credentials into an independent jar", "[session]") {
    Fixture f;
    auto session = f.profile->make_session(f.services);
    REQUIRE(session.set_mirror("https://custom.example"));
    auto authenticated = co_await f.profile->authenticate(session, AuthenticationToken{"saved", {}});
    REQUIRE(authenticated);
    authenticated->config()->cookie_jar->set_cookie({.name = "transient", .value = "omit", .domain = "api.example"});
    auto saved = authenticated->export_state();
    CHECK(saved.parser_id == "session-test");
    REQUIRE(saved.auth.cookies.size() == 1);
    CHECK_FALSE(saved.auth.headers.contains("X-Protocol"));
    auto restored = f.profile->restore_session(f.services, saved);
    REQUIRE(restored);
    CHECK(restored->snapshot().base_url() == "https://custom.example");
    CHECK(restored->config()->headers.at("X-Protocol") == "fixed");
    CHECK(restored->config()->headers.at("Authorization") == "saved");
    CHECK(restored->config()->cookie_jar != authenticated->config()->cookie_jar);
    saved.profile_id = "another";
    CHECK_FALSE(f.profile->restore_session(f.services, saved));
}

CORO_TEST_CASE("Web login retains its original frontend across browser checks", "[session]") {
    Fixture f;
    auto current = f.profile->make_session(f.services);
    auto login = f.profile->begin_web_login(current);
    CHECK_FALSE(login.set_mirror("https://another.example"));
    CHECK_FALSE(login.set_settings({}));
    auto unpinned = co_await f.profile->check_web_login(current, WebLoginEventKind::StorageChanged, {});
    REQUIRE_FALSE(unpinned);
    CHECK(unpinned.error().code == RequestErrorCode::InvalidArguments);
    auto info = f.profile->auth_info(login);
    REQUIRE(info);
    f.services->overrides->store(generation("https://new-api.example", "https://new-web.example"));
    WebLoginState browser;
    browser.storage.push_back({{"https://web.example", WebStorageKind::Local, "token"}, "browser"});
    auto result = co_await f.profile->check_web_login(login, WebLoginEventKind::StorageChanged, browser);
    REQUIRE(result);
    auto completed = std::get_if<InteractionCompleted<ProfileSession>>(&*result);
    REQUIRE(completed);
    CHECK(completed->value.config()->headers.at("Authorization") == "browser");
    CHECK(current.config()->cookie_jar->cookies().empty());
    CHECK(login.snapshot().canonical_base_url() == info->web_login->login_url);
    CHECK(completed->value.snapshot().canonical_base_url() == "https://new-web.example");
}

namespace {
struct SessionSelectors {
    std::string marker;
    static SessionSelectors make(const html::SelectorSource& source) {
        return {std::string(source.get("marker", "builtin"))};
    }
};
struct SessionPatterns {
    std::string marker;
    static SessionPatterns make(const RegexSource& source) {
        return {std::string(source.get("marker", "builtin"))};
    }
};
}

TEST_CASE("Snapshots compile selectors and patterns from one retained generation", "[session]") {
    Fixture f;
    auto publish = [&](std::string value) {
        auto selectors = std::make_shared<const html::SelectorSource>(
            html::SelectorSource::Overrides{{"marker", value}});
        auto patterns = std::make_shared<const RegexSource>(RegexSource::Overrides{{"marker", value}});
        auto origins = generation("https://" + value + ".example", "https://web-" + value + ".example");
        f.services->overrides->store(std::make_shared<const OverridesState>(origins->mirrors, selectors, patterns));
    };
    publish("old");
    auto session = f.profile->make_session(f.services);
    auto old = session.snapshot();
    publish("new");
    auto next = session.snapshot();
    // Compile the old generation only after the new one was published.
    CHECK(old.selectors<SessionSelectors>()->marker == "old");
    CHECK(old.patterns<SessionPatterns>()->marker == "old");
    CHECK(next.selectors<SessionSelectors>()->marker == "new");
    CHECK(next.patterns<SessionPatterns>()->marker == "new");
    CHECK(next.base_url() == "https://new.example");
    CHECK(old.base_url() == "https://old.example");
    CHECK(session.snapshot().selectors<SessionSelectors>() == next.selectors<SessionSelectors>());
}

CORO_TEST_CASE("Cancelled authentication never publishes its candidate", "[session]") {
    Fixture f;
    auto current = f.profile->make_session(f.services);
    auto jar = current.config()->cookie_jar;
    f.client->failure = RequestErrorCode::Cancelled;
    auto result = co_await f.profile->authenticate(current, AuthenticationToken{"cancelled", {}});
    REQUIRE_FALSE(result);
    CHECK(result.error().code == RequestErrorCode::Cancelled);
    CHECK(current.config()->cookie_jar == jar);
    CHECK(jar->cookies().empty());
    CHECK_FALSE(current.config()->headers.contains("Authorization"));
}

CORO_TEST_CASE("User request settings survive login and persistence independently of credentials", "[session]") {
    Fixture f;
    SessionConfig settings;
    settings.headers.set("X-Choice", "keep");
    settings.headers.set("X-Protocol", "fixed"); // Explicit even when equal to today's default.
    settings.url_params["locale"] = "ru";
    auto session = f.profile->make_session(f.services, settings);
    auto authenticated = co_await f.profile->authenticate(session, AuthenticationToken{"account", {}});
    REQUIRE(authenticated);
    CHECK(authenticated->config()->headers.at("X-Choice") == "keep");
    CHECK(authenticated->config()->url_params.at("locale") == "ru");
    auto saved = authenticated->export_state();
    CHECK(saved.settings == settings);
    CHECK_FALSE(saved.settings.headers.contains("X-Transient"));
    auto restored = f.profile->restore_session(f.services, saved);
    REQUIRE(restored);
    CHECK(restored->settings() == settings);
    CHECK(restored->config()->headers.at("X-Choice") == "keep");
    CHECK_FALSE(restored->config()->headers.contains("X-Transient"));
    auto old = authenticated->snapshot();
    REQUIRE(authenticated->set_settings({}));
    CHECK_FALSE(authenticated->config()->headers.contains("X-Choice"));
    CHECK(authenticated->config()->headers.at("X-Protocol") == "fixed");
    CHECK(authenticated->config()->headers.at("X-Transient") == "login-state");
    CHECK(authenticated->config()->headers.at("Authorization") == "account");
    CHECK(authenticated->config()->url_params.at("key") == "account");
    CHECK_FALSE(authenticated->config()->url_params.contains("locale"));
    CHECK(authenticated->config()->cookie_jar == old.config()->cookie_jar);
    CHECK(old.config()->headers.at("X-Choice") == "keep");
    settings.headers.set("authorization", "not-an-option");
    CHECK_FALSE(authenticated->set_settings(settings));
    settings.headers.erase("authorization");
    settings.url_params["key"] = "not-an-option";
    CHECK_FALSE(authenticated->set_settings(settings));
    CHECK(authenticated->settings() == SessionConfig{});
}
