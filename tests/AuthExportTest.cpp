/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <aniparse/source/Parser.hpp>
#include <aniparse/runtime/RequestContext.hpp>
#include <aniparse/source/Authentication.hpp>

#include <memory>

using namespace aniparse;

namespace {

// Minimal vector-backed jar so the test exercises the export/import logic
// without pulling in the curl-backed CurlCookieJar (which needs a live client).
class FakeCookieJar : public CookieJar {
public:
	std::optional<Cookie> find_cookie(std::string_view name) const override {
		for (const Cookie& cookie : cookies_) {
			if (cookie.name == name) {
				return cookie;
			}
		}
		return std::nullopt;
	}

	std::vector<Cookie> cookies() const override {
		return cookies_;
	}

	void set_cookie(const Cookie& cookie) override {
		for (Cookie& existing : cookies_) {
			if (existing.name == cookie.name && existing.domain == cookie.domain &&
			    existing.path == cookie.path) {
				existing = cookie;
				return;
			}
		}
		cookies_.push_back(cookie);
	}

	void clear() override {
		cookies_.clear();
	}

	std::vector<std::string> serialize() const override {
		return {};
	}

	void deserialize(std::span<std::string>) override {
	}

private:
	std::vector<Cookie> cookies_;
};

// Implements only the pure-virtual surface; keeps the default auth_keys()
// (empty) and default export_auth().
struct StubProfile : ParserProfile {
    StubProfile() : ParserProfile("stub", {"main", "Default"}) {}
};

// Declares exactly which cookie / header / url-param entries are the durable
// credential, so export_auth() lifts those and leaves the volatile rest.
struct CredentialProfile : StubProfile {
	AuthKeys auth_keys() const noexcept override {
		AuthKeys keys;
		keys.cookies    = {"session"};
		keys.headers    = {"Authorization"};
		keys.url_params = {"token"};
		return keys;
	}
};

Cookie make_cookie(std::string name, std::string value) {
	Cookie cookie;
	cookie.name   = std::move(name);
	cookie.value  = std::move(value);
	cookie.domain = "example.com";
	return cookie;
}

ParserConfig make_authed_config() {
	ParserConfig config;
	config.cookie_jar = std::make_shared<FakeCookieJar>();
	config.cookie_jar->set_cookie(make_cookie("session", "secret"));
	config.cookie_jar->set_cookie(make_cookie("antibot", "volatile"));
	config.headers.set("Authorization", "Bearer xyz");
	config.headers.set("User-Agent", "volatile-agent");
	config.url_params["token"]      = "kept";
	config.url_params["ts"]         = "dropped";
	config.alt_link                 = 3;
	return config;
}

} // namespace

TEST_CASE("export_auth keeps only the credential-bearing entries") {
	CredentialProfile parser;
	ParserConfig config = make_authed_config();

	AuthState state = parser.export_auth(config);

	REQUIRE(state.cookies.size() == 1);
	REQUIRE(state.cookies[0].name == "session");
	REQUIRE(state.cookies[0].value == "secret");

	REQUIRE(state.headers.size() == 1);
	REQUIRE(state.headers.at("Authorization") == "Bearer xyz");

	REQUIRE(state.url_params.size() == 1);
	REQUIRE(state.url_params.at("token") == "kept");

	REQUIRE(state.alt_link == 3);
}

TEST_CASE("export_auth then import_auth round-trips the session into a fresh config") {
	CredentialProfile parser;
	ParserConfig source = make_authed_config();
	source.alt_link     = 7;

	AuthState state = parser.export_auth(source);

	ParserConfig restored;
	restored.cookie_jar = std::make_shared<FakeCookieJar>();
	import_auth(restored, state);

	// Only the durable credential crossed over; volatile entries stayed behind.
	REQUIRE(restored.cookie_jar->cookies().size() == 1);
	auto session = restored.cookie_jar->find_cookie("session");
	REQUIRE(session.has_value());
	REQUIRE(session->value == "secret");
	REQUIRE_FALSE(restored.cookie_jar->find_cookie("antibot").has_value());

	REQUIRE(restored.headers.at("Authorization") == "Bearer xyz");
	REQUIRE_FALSE(restored.headers.contains("User-Agent"));

	REQUIRE(restored.url_params.at("token") == "kept");
	REQUIRE_FALSE(restored.url_params.contains("ts"));

	REQUIRE(restored.alt_link == 7);
}

TEST_CASE("export_auth with no declared keys yields an empty session but keeps alt_link") {
	StubProfile parser;
	ParserConfig config = make_authed_config();
	config.alt_link     = 5;

	AuthState state = parser.export_auth(config);

	REQUIRE(state.cookies.empty());
	REQUIRE(state.headers.empty());
	REQUIRE(state.url_params.empty());
	REQUIRE(state.alt_link == 5);
}

TEST_CASE("export_auth tolerates a config without a cookie jar") {
	CredentialProfile parser;
	ParserConfig config;
	config.headers.set("Authorization", "Bearer xyz");

	AuthState state = parser.export_auth(config);

	REQUIRE(state.cookies.empty());
	REQUIRE(state.headers.at("Authorization") == "Bearer xyz");
}

TEST_CASE("import_auth without a cookie jar skips cookies but restores the rest") {
	AuthState state;
	state.cookies.push_back(make_cookie("session", "secret"));
	state.headers.set("Authorization", "Bearer xyz");
	state.url_params["token"]      = "kept";
	state.alt_link                 = 4;

	ParserConfig config; // no cookie jar provisioned
	import_auth(config, state);

	REQUIRE(config.headers.at("Authorization") == "Bearer xyz");
	REQUIRE(config.url_params.at("token") == "kept");
	REQUIRE(config.alt_link == 4);
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
    StorageProfile() : ParserProfile("example", {"main", "Account"}) {}
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
    RequestTask<std::shared_ptr<const ParserConfig>> authenticate(
        RequestContext context, AuthenticationData credentials) const override {
        ++exchanges;
        const auto& token = std::get<AuthenticationToken>(credentials).token;
        if (token == "rejected") {
            co_return make_request_error(RequestErrorCode::InvalidCredentials, "Rejected");
        }
        if (token == "empty") co_return std::shared_ptr<const ParserConfig>{};
        auto config = std::make_shared<ParserConfig>(*context.config());
        config->cookie_jar.reset();
        config->headers.set("Authorization", "Bearer " + token);
        co_return context.new_with_config(config).config();
    }
};
}

CORO_TEST_CASE("Web login verifies storage tokens only at the declared origin", "[auth][web-login]") {
    StorageProfile profile;
    auto context = profile.make_context(std::make_shared<WebLoginClient>());
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
    const auto* completed = std::get_if<InteractionCompleted<std::shared_ptr<const ParserConfig>>>(&*verified);
    REQUIRE(completed);
    REQUIRE(completed->value);
    CHECK(completed->value->headers.at("Authorization") == "Bearer valid");
    CHECK_FALSE(context.config()->headers.contains("Authorization"));
    CHECK(completed->value->cookie_jar != context.config()->cookie_jar);
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
        PasswordProfile() : ParserProfile("example", {"password", "Account"}) {}
        std::optional<AuthInfo> auth_info() const override { return AuthInfo{}; }
    } profile;
    REQUIRE_FALSE(profile.auth_info()->web_login);
    const auto context = profile.make_context(std::make_shared<WebLoginClient>());
    const auto observation = profile.web_login_observation(context);
    CHECK(observation.events.none());
    CHECK(observation.storage.empty());
    CHECK(observation.interval_ms == 0);
    const auto checked = co_await profile.check_web_login(context, WebLoginEventKind::Started, {});
    REQUIRE_FALSE(checked);
    CHECK(checked.error().code == RequestErrorCode::NotImplemented);
}
