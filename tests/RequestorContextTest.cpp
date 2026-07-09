/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include <aniparse/Client.hpp>
#include <aniparse/Parser.hpp>
#include <aniparse/manga/Manga.hpp>
#include <aniparse/html/HTMLDocument.hpp>

#include <boost/json.hpp>

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

struct DummyLogger : public LoggerContext {

	void log(LogLevel message_type,
		std::string_view message,
		const std::source_location loc = std::source_location::current()) override {

	}
};

// The single mock for every RequestorContext test. Now that do_request returns an
// expected there is no need to throw to abort: it records the configured request
// it received (for the routing/config tests) and co_returns a preset response (for
// the typed-helper tests). The captured request keeps its cookies, so tests can
// assert both the request value and the jar it was routed onto.
struct CannedClientMock : ClientContext {
	explicit CannedClientMock(std::shared_ptr<CookieJar> cookie_jar = nullptr,
	                          Response<ResponseData> response = ResponseData{ .status_code = 200 })
		: cookie_jar(std::move(cookie_jar))
		, response(std::move(response)) {}

	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	void set_config(ClientConfig config) override {
		this->config = std::make_unique<ClientConfig>(config);
	}
	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return cookie_jar;
	}

	std::variant<std::monostate,
		ConfiguredGetRequest,
		ConfiguredPostRequest,
		ConfiguredPostMultipartRequest> request;
	std::shared_ptr<CookieJar> cookie_jar;
	std::unique_ptr<ClientConfig> config;
	Response<ResponseData> response;
};

static RequestorContext canned_context(Response<ResponseData> response) {
	return RequestorContext(
		std::make_shared<CannedClientMock>(std::make_shared<DummyCookieJar>(), std::move(response)),
		std::make_shared<DummyLogger>(),
		std::make_shared<ParserConfig>());
}

// Client whose make_cookie_jar() hands out a fresh jar on every call, matching
// AsyncClient's contract (each call -> new CurlCookieJar). The base mock returns
// a fixed jar so other tests can assert against it; here we need distinct jars.
struct FreshJarClientMock : CannedClientMock {
	using CannedClientMock::CannedClientMock;

	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return std::make_shared<DummyCookieJar>();
	}
};

// Minimal concrete root getter: MangaRootGetter is abstract (from_serialized is
// pure). Everything else uses the base implementation, which is what we test.
struct DummyMangaRootGetter : MangaRootGetter {
	asyncnet::NetworkTask<Response<std::unique_ptr<MangaGetter>>> from_serialized(SerializedGetterData) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "n/a");
	}
};

struct DummyParser : Parser {
	ParserInfo info() const override { return { .name = "Dummy" }; }
	std::string identifier() const override { return "Dummy"; }
	ParserCompatibilities compatibilities() const override { return {}; }
	void emplace_domains(EmplaceDomainsContext&) const override {}
};

CORO_TEST_CASE("RequestorContext construct") {
	auto parser_config = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();
	auto client_logger = std::make_shared<DummyLogger>();

	auto mock_client = std::make_shared<CannedClientMock>(client_cookie_jar);

	REQUIRE_THROWS(RequestorContext(nullptr, nullptr, nullptr));
	REQUIRE_THROWS(RequestorContext(nullptr, client_logger, parser_config));
	REQUIRE_NOTHROW(RequestorContext(mock_client, nullptr, nullptr));
	REQUIRE_NOTHROW(RequestorContext(mock_client, client_logger, parser_config));

	RequestorContext context(mock_client, client_logger, parser_config);

	REQUIRE(context.config() == parser_config);

	co_return;
}

CORO_TEST_CASE("RequestorContext provisions a cookie jar when config has none") {
	auto parser_config     = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();
	auto mock_client       = std::make_shared<CannedClientMock>(client_cookie_jar);

	REQUIRE(parser_config->cookie_jar == nullptr);

	RequestorContext context(mock_client, nullptr, parser_config);

	// Empty config gets the client-provided jar.
	REQUIRE(context.config()->cookie_jar == client_cookie_jar);

	co_return;
}

CORO_TEST_CASE("RequestorContext keeps an existing cookie jar") {
	auto preset_jar        = std::make_shared<DummyCookieJar>();
	auto parser_config     = std::make_shared<ParserConfig>();
	parser_config->cookie_jar = preset_jar; // e.g. a session established earlier or a restored jar

	auto client_cookie_jar = std::make_shared<DummyCookieJar>();
	auto mock_client       = std::make_shared<CannedClientMock>(client_cookie_jar);

	RequestorContext context(mock_client, nullptr, parser_config);

	// The pre-existing jar must survive; it must not be replaced by make_cookie_jar().
	REQUIRE(context.config()->cookie_jar == preset_jar);
	REQUIRE(context.config()->cookie_jar != client_cookie_jar);

	co_return;
}

CORO_TEST_CASE("make_config does not inherit the base cookie jar") {
	DummyParser parser;

	auto base        = std::make_shared<ParserConfig>();
	base->cookie_jar = std::make_shared<DummyCookieJar>(); // pretend the base carries a live session

	auto derived = parser.make_config(base);

	REQUIRE(derived != nullptr);
	// A derived config belongs to a distinct instance and must not share the store.
	REQUIRE(derived->cookie_jar == nullptr);
	// make_config stamps the parser identity for request-time services.
	REQUIRE(derived->parser_id == "Dummy");

	// A null base yields a null config (nothing to derive from).
	REQUIRE(parser.make_config(nullptr) == nullptr);

	co_return;
}

CORO_TEST_CASE("RequestorContext isolates cookie jars across parser instances") {
	DummyParser parser;
	auto base = std::make_shared<ParserConfig>();

	auto config_a = parser.make_config(base);
	auto config_b = parser.make_config(base);

	auto client = std::make_shared<FreshJarClientMock>(std::make_shared<DummyCookieJar>());
	RequestorContext context_a(client, nullptr, config_a);
	RequestorContext context_b(client, nullptr, config_b);

	// Each instance provisions its own store; two instances never share cookies.
	REQUIRE(context_a.config()->cookie_jar != nullptr);
	REQUIRE(context_b.config()->cookie_jar != nullptr);
	REQUIRE(context_a.config()->cookie_jar != context_b.config()->cookie_jar);

	co_return;
}

CORO_TEST_CASE("RequestorContext GET with empty config") {
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

	RequestorContext context(mock_client, nullptr, parser_config);

	REQUIRE((co_await context.request(test_request)).has_value());

	auto check_request = [&]() {
		if (auto* request = std::get_if<ConfiguredGetRequest>(&mock_client->request)) {
			return request->request == test_request && request->cookies == client_cookie_jar;
		}
		return false;
	};

	REQUIRE(check_request());
}

CORO_TEST_CASE("RequestorContext GET with normal config") {
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
	parser_config->modifiers.push_back([](ClientRequest& any_request) {
		if (auto* request = std::get_if<GetRequest>(&any_request)) {
			request->url.insert(0, "/v1");
			//request->headers["User-Agent"] += "/1.0";
		}
	});

	auto client_cookie_jar = std::make_shared<DummyCookieJar>();

	auto mock_client = std::make_shared<CannedClientMock>(client_cookie_jar);

	RequestorContext context(mock_client, nullptr, parser_config);

	REQUIRE((co_await context.request(test_request)).has_value());

	auto check_request = [&]<typename ExpectedRequest>(const ExpectedRequest & expected_request) {
		if (auto* request = std::get_if<ConfiguredRequest<ExpectedRequest>>(&mock_client->request)) {
			return request->request == expected_request && request->cookies == client_cookie_jar;
		}
		return false;
	};

	REQUIRE(check_request(expected_test_request));
}

CORO_TEST_CASE("RequestorContext all with empty config") {
	GetRequest test_get_request = {};
	PostRequest test_post_request = {};
	PostMultipartRequest test_post_multipart_request = {};

	auto parser_config = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();

	auto mock_client = std::make_shared<CannedClientMock>(client_cookie_jar);

	RequestorContext context(mock_client, nullptr, parser_config);

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

CORO_TEST_CASE("RequestorContext request headers take priority over config") {
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

	RequestorContext context(mock_client, nullptr, parser_config);

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

CORO_TEST_CASE("request carries the HTTP method through to the client") {
	auto mock_client = std::make_shared<CannedClientMock>(std::make_shared<DummyCookieJar>());
	RequestorContext context(mock_client, nullptr, std::make_shared<ParserConfig>());

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
	auto context = canned_context(ResponseData{
		.status_code = 200,
		.body        = "<!DOCTYPE html><html><head><title>Hi</title></head><body></body></html>" });

	auto result = co_await context.request_html(GetRequest{ .url = "/x" });

	// The helper's job: a 2xx + parseable body yields a document. HTML parsing
	// correctness itself lives in HTMLParseTests.
	REQUIRE(result.has_value());
}

CORO_TEST_CASE("request_json parses a 2xx JSON body") {
	auto context = canned_context(ResponseData{
		.status_code = 200,
		.body        = R"({"answer": 42})" });

	auto result = co_await context.request_json(GetRequest{ .url = "/x" });

	REQUIRE(result.has_value());
	REQUIRE(result->as_object().at("answer").as_int64() == 42);
}

CORO_TEST_CASE("request_json works over POST") {
	auto context = canned_context(ResponseData{
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
		auto context = canned_context(ResponseData{ .status_code = status, .body = "<html></html>" });

		auto result = co_await context.request_html(GetRequest{ .url = "/x" });

		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code == code);
		REQUIRE(result.error().http_status == status);
	}
}

CORO_TEST_CASE("typed helpers map a malformed 2xx body to UnexpectedResponse") {
	auto context = canned_context(ResponseData{ .status_code = 200, .body = "{not valid json" });

	auto result = co_await context.request_json(GetRequest{ .url = "/x" });

	REQUIRE_FALSE(result.has_value());
	REQUIRE(result.error().code == RequestErrorCode::UnexpectedResponse);
	REQUIRE(result.error().http_status == 200); // the fetch succeeded; only the parse failed
}

CORO_TEST_CASE("typed helpers forward a transport RequestError unchanged") {
	auto context = canned_context(make_response_error(RequestErrorCode::NetworkError, "boom"));

	auto result = co_await context.request_html(GetRequest{ .url = "/x" });

	REQUIRE_FALSE(result.has_value());
	REQUIRE(result.error().code == RequestErrorCode::NetworkError);
}