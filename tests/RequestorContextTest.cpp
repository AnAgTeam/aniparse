/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "TestRequestContext.hpp"
#include "CoroTest.hpp"
#include <aniparse/net/backends/curl/CurlHttpClient.hpp>
#include <aniparse/source/Parser.hpp>
#include <aniparse/manga/MangaGetter.hpp>
#include <aniparse/html/HTMLDocument.hpp>

#include <boost/json.hpp>
#include <aniparse/net/CookieStore.hpp>
#include <type_traits>

using namespace aniparse;
using namespace std::string_view_literals;

struct DummyCookieJar : public CookieJar {

	std::optional<Cookie> find_cookie(std::string_view name) const override {
		return std::nullopt;
	}

	std::vector<Cookie> cookies() const override {
		return {};
	}

	void set_cookie(const Cookie& cookie) override {

	}

	void clear() override {

	}

	std::vector<std::string> serialize() const override {
		return {};
	}

	void deserialize(std::span<std::string> cookies) override {

	}
};

struct DummyLogger : public Logger {

	void log(LogLevel message_type,
		std::string_view message,
		std::string_view parser_id,
		const std::source_location loc = std::source_location::current()) override {

	}
};

// The single mock for every RequestContext test. Now that do_request returns an
// expected there is no need to throw to abort: it records the configured request
// it received (for the routing/config tests) and co_returns a preset response (for
// the typed-helper tests). The captured request keeps its cookies, so tests can
// assert both the request value and the jar it was routed onto.
struct CannedClientMock : HttpClient {
	explicit CannedClientMock(std::shared_ptr<CookieJar> cookie_jar = nullptr,
	                          Result<HttpResponse> response = HttpResponse{ .status_code = 200 })
		: cookie_jar(std::move(cookie_jar))
		, response(std::move(response)) {}

	RequestTask<HttpResponse> do_request(ConfiguredGetRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	void apply_client_config(HttpClientConfig config) override {
		this->config = std::make_unique<HttpClientConfig>(config);
	}
	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return cookie_jar;
	}

	std::variant<std::monostate,
		ConfiguredGetRequest,
		ConfiguredPostRequest,
		ConfiguredPostMultipartRequest> request;
	std::shared_ptr<CookieJar> cookie_jar;
	std::unique_ptr<HttpClientConfig> config;
	Result<HttpResponse> response;
};

static RequestContext canned_context(Result<HttpResponse> response) {
	return test_context(
		std::make_shared<CannedClientMock>(std::make_shared<DummyCookieJar>(), std::move(response)),
		std::make_shared<DummyLogger>(),
		std::make_shared<ParserConfig>());
}

// Client whose make_cookie_jar() hands out a fresh jar on every call, matching
// CurlHttpClient's contract (each call -> new CurlCookieJar). The base mock returns
// a fixed jar so other tests can assert against it; here we need distinct jars.
struct FreshJarClientMock : CannedClientMock {
	using CannedClientMock::CannedClientMock;

	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return std::make_shared<DummyCookieJar>();
	}
};



struct DummyParser : Parser {
	ParserInfo info() const override { return { .name = "Dummy" }; }
	std::string identifier() const override { return "Dummy"; }
	ParserCapabilities capabilities() const override { return {}; }
	void emplace_domains(EmplaceDomainsSink&) const override {}
};

CORO_TEST_CASE("RequestContext construct") {
	auto parser_config = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();
	auto client_logger = std::make_shared<DummyLogger>();

	auto mock_client = std::make_shared<CannedClientMock>(client_cookie_jar);

	REQUIRE_THROWS(test_context(nullptr, nullptr, nullptr));
	REQUIRE_THROWS(test_context(nullptr, client_logger, parser_config));
	REQUIRE_NOTHROW(test_context(mock_client, nullptr, nullptr));
	REQUIRE_NOTHROW(test_context(mock_client, client_logger, parser_config));

	auto context = test_context(mock_client, client_logger, parser_config);

	REQUIRE(context.config() != parser_config);
	REQUIRE(context.config()->cookie_jar == client_cookie_jar);

	co_return;
}

CORO_TEST_CASE("RequestContext provisions a cookie jar when config has none") {
	auto parser_config     = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();
	auto mock_client       = std::make_shared<CannedClientMock>(client_cookie_jar);

	REQUIRE(parser_config->cookie_jar == nullptr);

	auto context = test_context(mock_client, nullptr, parser_config);

	// Empty config gets the client-provided jar.
	REQUIRE(context.config()->cookie_jar == client_cookie_jar);

	co_return;
}

CORO_TEST_CASE("Profile defaults cannot carry another account cookie jar") {
	auto preset_jar        = std::make_shared<DummyCookieJar>();
	auto parser_config     = std::make_shared<ParserConfig>();
	parser_config->cookie_jar = preset_jar; // e.g. a session established earlier or a restored jar

	auto client_cookie_jar = std::make_shared<DummyCookieJar>();
	auto mock_client       = std::make_shared<CannedClientMock>(client_cookie_jar);

	auto context = test_context(mock_client, nullptr, parser_config);

	// A new account always gets its backend jar, even if profile defaults carry a jar.
	REQUIRE(context.config()->cookie_jar != preset_jar);
	REQUIRE(context.config()->cookie_jar == client_cookie_jar);

	co_return;
}

CORO_TEST_CASE("make_session creates fresh independent profile settings") {
	DummyParser parser;

	auto derived = parser.make_profile("main")->make_session(test_services(std::make_shared<CannedClientMock>())).config();

	REQUIRE(derived != nullptr);
	// A derived config belongs to a distinct instance and must not share the store.
	REQUIRE(derived->cookie_jar == nullptr);
	// The parser identity is bound separately from the request configuration.

	REQUIRE_FALSE(derived->headers.contains("Authorization"));

	co_return;
}

// Records what it is handed, so a test can assert the emitter's identity travels
// with the message.
struct RecordingLogger : Logger {
	void log(LogLevel message_type,
	         std::string_view message,
	         std::string_view parser_id,
	         std::source_location) override {
		last_level     = message_type;
		last_message   = std::string(message);
		last_parser_id = std::string(parser_id);
	}

	LogLevel last_level = LogLevel::Info;
	std::string last_message;
	std::string last_parser_id;
};

CORO_TEST_CASE("RequestContext stamps the parser id on log messages") {
	DummyParser parser;
	auto logger = std::make_shared<RecordingLogger>();

	auto session = parser.make_profile("main")->make_session(test_services(std::make_shared<CannedClientMock>(), logger));
	auto original = session.snapshot();
	original.info("original");
	REQUIRE(logger->last_parser_id == "Dummy");

	// Publishing user settings cannot replace the private profile binding.
	REQUIRE(session.set_mirror("https://changed.example"));
	auto derived = session.snapshot();
	derived.error("derived");
	REQUIRE(logger->last_level == LogLevel::Error);
	REQUIRE(logger->last_parser_id == "Dummy");

	co_return;
}

CORO_TEST_CASE("RequestContext isolates cookie jars across parser instances") {
	DummyParser parser;
	auto config_a = parser.make_profile("main")->make_session(test_services(std::make_shared<CannedClientMock>())).config();
	auto config_b = parser.make_profile("main")->make_session(test_services(std::make_shared<CannedClientMock>())).config();

	auto client = std::make_shared<FreshJarClientMock>(std::make_shared<DummyCookieJar>());
	auto context_a = test_context(client, nullptr, config_a);
	auto context_b = test_context(client, nullptr, config_b);

	// Each instance provisions its own store; two instances never share cookies.
	REQUIRE(context_a.config()->cookie_jar != nullptr);
	REQUIRE(context_b.config()->cookie_jar != nullptr);
	REQUIRE(context_a.config()->cookie_jar != context_b.config()->cookie_jar);

	co_return;
}

CORO_TEST_CASE("RequestContext GET with empty config") {
	GetRequest test_request = {
		.url = "/api/test",
		.url_params = {
			{ "token", "mytoken" },
			{ "version", "1.0" },
		},
		.headers = {
			{ "User-Agent", "123" },
			{ "Authorization", "some-password" },
		},
	};

	auto parser_config = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();

	auto mock_client = std::make_shared<CannedClientMock>(client_cookie_jar);

	auto context = test_context(mock_client, nullptr, parser_config);

	REQUIRE((co_await context.request(test_request)).has_value());

	auto check_request = [&]() {
		if (auto* request = std::get_if<ConfiguredGetRequest>(&mock_client->request)) {
			return request->request == test_request && request->cookies == client_cookie_jar;
		}
		return false;
	};

	REQUIRE(check_request());
}

CORO_TEST_CASE("RequestContext GET with normal config") {
	GetRequest expected_test_request = {
		.url = "/v1/api/test",
		.url_params = {
			{ "token", "mytoken" },
			{ "version", "1.0" },
		},
		.headers = {
			{ "User-Agent", "123" },
			{ "Authorization", "some-password" },
		},
	};

	GetRequest test_request = {
		.url = "/api/test",
		.url_params = {
			{ "token", "mytoken" },
		},
		.headers = {
			{ "User-Agent", "123" },
		},
	};

	auto parser_config = std::make_shared<ParserConfig>();
	parser_config->headers.set("Authorization", "some-password");
	parser_config->url_params["version"] = "1.0";
	parser_config->modifiers.push_back([](HttpRequest& any_request, const RequestContext&) {
		if (auto* request = std::get_if<GetRequest>(&any_request)) {
			request->url.insert(0, "/v1");
			//request->headers["User-Agent"] += "/1.0";
		}
	});

	auto client_cookie_jar = std::make_shared<DummyCookieJar>();

	auto mock_client = std::make_shared<CannedClientMock>(client_cookie_jar);

	auto context = test_context(mock_client, nullptr, parser_config);

	REQUIRE((co_await context.request(test_request)).has_value());

	auto check_request = [&]<typename ExpectedRequest>(const ExpectedRequest & expected_request) {
		if (auto* request = std::get_if<ConfiguredRequest<ExpectedRequest>>(&mock_client->request)) {
			return request->request == expected_request && request->cookies == client_cookie_jar;
		}
		return false;
	};

	REQUIRE(check_request(expected_test_request));
}

CORO_TEST_CASE("RequestContext all with empty config") {
	GetRequest test_get_request = {};
	PostRequest test_post_request = {};
	PostMultipartRequest test_post_multipart_request = {};

	auto parser_config = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();

	auto mock_client = std::make_shared<CannedClientMock>(client_cookie_jar);

	auto context = test_context(mock_client, nullptr, parser_config);

	auto check_request = [&]<typename ExpectedRequest>(const ExpectedRequest& expected_request) {
		if (auto* request = std::get_if<ConfiguredRequest<ExpectedRequest>>(&mock_client->request)) {
			return request->request == expected_request && request->cookies == client_cookie_jar;
		}
		return false;
	};

	REQUIRE((co_await context.request(test_get_request)).has_value());
	REQUIRE(check_request(test_get_request));

	REQUIRE((co_await context.request(test_post_request)).has_value());
	REQUIRE(check_request(test_post_request));

	REQUIRE((co_await context.request(test_post_multipart_request)).has_value());
	REQUIRE(check_request(test_post_multipart_request));
}

CORO_TEST_CASE("RequestContext request headers take priority over config") {
	GetRequest test_request = {
		.url = "/api/test",
		.headers = {
			{ "Authorization", "from-request" },
		},
	};

	auto parser_config = std::make_shared<ParserConfig>();
	parser_config->headers.set("Authorization", "from-config"); // must lose to the request
	parser_config->headers.set("X-Extra", "config-only");       // must still be merged in

	auto client_cookie_jar = std::make_shared<DummyCookieJar>();
	auto mock_client = std::make_shared<CannedClientMock>(client_cookie_jar);

	auto context = test_context(mock_client, nullptr, parser_config);

	REQUIRE((co_await context.request(test_request)).has_value());

	auto* captured = std::get_if<ConfiguredGetRequest>(&mock_client->request);
	REQUIRE(captured != nullptr);

	const Headers& headers = captured->request.headers;
	// request value wins on collision
	REQUIRE(headers.at("Authorization") == "from-request");
	// config-only header still merged in
	REQUIRE(headers.at("X-Extra") == "config-only");
	// header names are matched case-insensitively
	REQUIRE(headers.contains("authorization"));
}

CORO_TEST_CASE("Request modifiers share the operation snapshot for every request kind", "[session]") {
    auto config = std::make_shared<ParserConfig>();
    config->headers.set("X-Profile", "profile");
    config->modifiers.emplace_back([](HttpRequest& request, const RequestContext& context) {
        std::visit([&](auto& value) {
            CHECK(value.headers.at("X-Profile") == "profile");
            CHECK(value.headers.at("X-App") == "original");
            value.headers.set("Referer", std::string(context.canonical_base_url()));
        }, request);
    });
    session_test::Factory parser;
    parser.factory = [config](auto info) {
        return std::make_unique<session_test::ConfiguredProfile>(std::move(info), config);
    };
    auto client = std::make_shared<CannedClientMock>();
    HttpClientConfig defaults;
    defaults.request_defaults.headers.set("X-App", "original");
    client->set_config(defaults);
    SessionConfig settings;
    settings.canonical_base_url = "https://original.example";
    auto session = parser.make_profile("main")->make_session(test_services(client), settings);
    auto context = session.snapshot();
    settings.canonical_base_url = "https://new.example";
    REQUIRE(session.set_settings(std::move(settings)));
    defaults.request_defaults.headers.set("X-App", "new");
    client->set_config(std::move(defaults));

    auto check_referer = [&] {
        std::visit([](const auto& request) {
            if constexpr (std::is_same_v<std::decay_t<decltype(request)>, std::monostate>) {
                FAIL("Client did not receive a request");
            } else {
                CHECK(request.request.headers.at("Referer") == "https://original.example");
            }
        }, client->request);
    };
    REQUIRE(co_await context.request(GetRequest{}));
    check_referer();
    REQUIRE(co_await context.request(PostRequest{}));
    check_referer();
    REQUIRE(co_await context.request(PostMultipartRequest{}));
    check_referer();
}

CORO_TEST_CASE("request carries the HTTP method through to the client") {
	auto mock_client = std::make_shared<CannedClientMock>(std::make_shared<DummyCookieJar>());
	auto context = test_context(mock_client, nullptr, std::make_shared<ParserConfig>());

	// GET defaults to Get.
	REQUIRE((co_await context.request(GetRequest{ .url = "https://example.com" })).has_value());
	{
		auto* captured = std::get_if<ConfiguredGetRequest>(&mock_client->request);
		REQUIRE(captured != nullptr);
		REQUIRE(captured->request.method == HttpMethod::Get);
	}

	// An explicit verb survives config application and routing.
	REQUIRE((co_await context.request(GetRequest{ .url = "https://example.com", .method = HttpMethod::Head })).has_value());
	{
		auto* captured = std::get_if<ConfiguredGetRequest>(&mock_client->request);
		REQUIRE(captured != nullptr);
		REQUIRE(captured->request.method == HttpMethod::Head);
	}

	// POST defaults to Post.
	REQUIRE((co_await context.request(PostRequest{ .url = "https://example.com" })).has_value());
	{
		auto* captured = std::get_if<ConfiguredPostRequest>(&mock_client->request);
		REQUIRE(captured != nullptr);
		REQUIRE(captured->request.method == HttpMethod::Post);
	}
}

CORO_TEST_CASE("request_html parses a 2xx HTML body") {
	auto context = canned_context(HttpResponse{
		.status_code = 200,
		.body        = "<!DOCTYPE html><html><head><title>Hi</title></head><body></body></html>" });

	auto result = co_await context.request_html(GetRequest{ .url = "/x" });

	// The helper's job: a 2xx + parseable body yields a document. HTML parsing
	// correctness itself lives in HTMLParseTests.
	REQUIRE(result.has_value());
}

CORO_TEST_CASE("request_json parses a 2xx JSON body") {
	auto context = canned_context(HttpResponse{
		.status_code = 200,
		.body        = R"({"answer": 42})" });

	auto result = co_await context.request_json(GetRequest{ .url = "/x" });

	REQUIRE(result.has_value());
	REQUIRE(result->as_object().at("answer").as_int64() == 42);
}

CORO_TEST_CASE("request_json works over POST") {
	auto context = canned_context(HttpResponse{
		.status_code = 200,
		.body        = R"({"ok": true})" });

	auto result = co_await context.request_json(PostRequest{ .url = "/x", .body = "payload" });

	REQUIRE(result.has_value());
	REQUIRE(result->as_object().at("ok").as_bool() == true);
}

CORO_TEST_CASE("typed helpers map HTTP status to a RequestError code") {
	struct Case { long status; RequestErrorCode code; };
	for (auto [status, code] : { Case{ 401, RequestErrorCode::InvalidCredentials },
	                             Case{ 403, RequestErrorCode::InvalidCredentials },
	                             Case{ 404, RequestErrorCode::NotFound },
	                             Case{ 429, RequestErrorCode::RateLimited },
	                             Case{ 500, RequestErrorCode::ServerError },
	                             Case{ 503, RequestErrorCode::ServerError } }) {
		auto context = canned_context(HttpResponse{ .status_code = status, .body = "<html></html>" });

		auto result = co_await context.request_html(GetRequest{ .url = "/x" });

		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code == code);
		REQUIRE(result.error().http_status == status);
	}
}

CORO_TEST_CASE("typed helpers map a malformed 2xx body to UnexpectedResponse") {
	auto context = canned_context(HttpResponse{ .status_code = 200, .body = "{not valid json" });

	auto result = co_await context.request_json(GetRequest{ .url = "/x" });

	REQUIRE_FALSE(result.has_value());
	REQUIRE(result.error().code == RequestErrorCode::UnexpectedResponse);
	REQUIRE(result.error().http_status == 200); // the fetch succeeded; only the parse failed
}

CORO_TEST_CASE("typed helpers forward a transport RequestError unchanged") {
	auto context = canned_context(make_request_error(RequestErrorCode::NetworkError, "boom"));

	auto result = co_await context.request_html(GetRequest{ .url = "/x" });

	REQUIRE_FALSE(result.has_value());
	REQUIRE(result.error().code == RequestErrorCode::NetworkError);
}
CORO_TEST_CASE("Request defaults respect header precedence for every HTTP request kind", "[profiles]") {
    auto client = std::make_shared<CannedClientMock>();
    HttpClientConfig client_config;
    client_config.request_defaults.headers.set("User-Agent", "App/1.0");
    client_config.request_defaults.headers.set("X-Default", "fallback");
    client_config.request_defaults.headers.set("X-Source", "fallback");
    client->set_config(client_config);

    auto config = std::make_shared<ParserConfig>();
    config->headers.set("user-agent", "Source/1.0");
    config->headers.set("x-source", "source");
    auto context = test_context(client, nullptr, config);

    enum class Method { Get, Post, Multipart };
    for (auto method : {Method::Get, Method::Post, Method::Multipart}) {
        Headers headers{{"USER-AGENT", "Request/1.0"}};
        switch (method) {
        case Method::Get:
            co_await context.request(GetRequest{.url = "https://example.com", .headers = headers});
            break;
        case Method::Post:
            co_await context.request(PostRequest{.url = "https://example.com", .headers = headers});
            break;
        case Method::Multipart:
            co_await context.request(PostMultipartRequest{.url = "https://example.com", .headers = headers});
            break;
        }
        std::visit([](const auto& sent) {
            if constexpr (requires { sent.request.headers; }) {
                CHECK(sent.request.headers.at("User-Agent") == "Request/1.0");
                CHECK(sent.request.headers.at("X-Source") == "source");
                CHECK(sent.request.headers.at("X-Default") == "fallback");
                CHECK(sent.request.headers.size() == 3);
            }
        }, client->request);
    }

    co_await context.request(GetRequest{.url = "https://example.com"});
    CHECK(std::get<ConfiguredGetRequest>(client->request).request.headers.at("User-Agent") == "Source/1.0");
    CHECK_FALSE(config->headers.contains("X-Default"));

    config->headers = {};
    context = test_context(client, nullptr, config);
    client_config.request_defaults.headers.set("User-Agent", "App/2.0");
    client->set_config(client_config);
    co_await context.request(GetRequest{.url = "https://example.com"});
    CHECK(std::get<ConfiguredGetRequest>(client->request).request.headers.at("User-Agent") == "App/1.0");
    auto next = test_context(client, nullptr, config);
    co_await next.request(GetRequest{.url = "https://example.com"});
    CHECK(std::get<ConfiguredGetRequest>(client->request).request.headers.at("User-Agent") == "App/2.0");
    client->set_config({});
    co_await context.request(GetRequest{.url = "https://example.com"});
    CHECK(std::get<ConfiguredGetRequest>(client->request).request.headers.at("User-Agent") == "App/1.0");
    auto cleared = test_context(client, nullptr, config);
    co_await cleared.request(GetRequest{.url = "https://example.com"});
    CHECK_FALSE(std::get<ConfiguredGetRequest>(client->request).request.headers.contains("User-Agent"));
    co_return;
}

namespace {
template <class T>
concept HasPublicParserIdentity = requires(T value) { value.parser_id; };
template <class T>
concept HasPublicProfileIdentity = requires(T value) { value.profile_id; };

static_assert(!std::is_default_constructible_v<RequestContext>);
static_assert(!std::is_constructible_v<RequestContext, std::shared_ptr<HttpClient>>);
static_assert(!std::is_constructible_v<RequestContext,
              std::shared_ptr<const RuntimeServices>, std::shared_ptr<ParserConfig>>);
static_assert(!HasPublicParserIdentity<ParserConfig>);
static_assert(!HasPublicProfileIdentity<ParserConfig>);
}

TEST_CASE("Session settings are copied and retain the profile binding", "[profiles]") {
    auto profile = test_profile("Example");
    auto session = profile->make_session(test_services());
    auto context = session.snapshot();
    SessionConfig settings;
    settings.headers.set("X-Account", "new");
    REQUIRE(session.set_settings(settings));
    auto updated = session.snapshot();
    settings.headers.set("X-Account", "later");
    CHECK(updated.config()->headers.at("X-Account") == "new");
    CHECK_FALSE(context.config()->headers.contains("X-Account"));
    static_assert(!std::is_constructible_v<ParserProfile, std::string, ProfileInfo>);
}

TEST_CASE("Profile sessions restore credentials over fresh defaults and backend jars", "[profiles]") {
    struct Profile : ParserProfile {
        using ParserProfile::ParserProfile;
        void configure(ParserConfig& config) const override { config.headers.set("X-Default", "current"); }
        AuthKeys auth_keys() const noexcept override { return {{"session"}, {"Authorization"}, {"token"}}; }
    };
    auto profile = test_profile<Profile>("Example", {"account", "Account"});
    auto services = test_services();
    auto original = profile->make_session(services);
    auto saved = original.export_state();
    saved.auth.headers.set("Authorization", "Bearer saved");
    saved.auth.url_params["token"] = "saved";
    saved.settings.base_url = "https://selected.example";
    auto cookie = parse_netscape_line("example.com\tFALSE\t/\tFALSE\t0\tsession\tsaved");
    REQUIRE(cookie);
    saved.auth.cookies.push_back(*cookie);
    auto restored = profile->restore_session(services, saved);
    REQUIRE(restored);
    CHECK(restored->config()->cookie_jar != original.config()->cookie_jar);
    CHECK(restored->config()->headers.at("X-Default") == "current");
    CHECK(restored->config()->headers.at("Authorization") == "Bearer saved");
    CHECK(restored->config()->url_params.at("token") == "saved");
    CHECK(restored->snapshot().base_url() == *saved.settings.base_url);
    auto restored_cookie = restored->config()->cookie_jar->find_cookie("session");
    REQUIRE(restored_cookie);
    CHECK(restored_cookie->value == "saved");
    CHECK_FALSE(original.config()->headers.contains("Authorization"));
}
