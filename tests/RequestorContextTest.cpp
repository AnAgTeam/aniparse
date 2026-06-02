/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include <aniparse/Client.hpp>

using namespace aniparse;
using namespace std::string_view_literals;

struct MockSuccess : std::runtime_error {
	using std::runtime_error::runtime_error;
};

struct ClientContextMock : ClientContext {

	ClientContextMock(std::shared_ptr<CookieJar> cookie_jar) : cookie_jar(cookie_jar) {}
	
	asyncnet::NetworkTask<asyncnet::Response> do_request(ConfiguredGetRequest request) override {
		this->request = request;
		throw MockSuccess("Success");
	}

	asyncnet::NetworkTask<asyncnet::Response> do_request(ConfiguredPostRequest request) override {
		this->request = request;
		throw MockSuccess("Success");
	}

	asyncnet::NetworkTask<asyncnet::Response> do_request(ConfiguredPostMultipartRequest request) override {
		this->request = request;
		throw MockSuccess("Success");
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
};

struct DummyCookieJar : public CookieJar {

	std::optional<std::string> find_cookie(std::string_view name) const override {
		return std::nullopt;
	}

	void set_cookie(std::string cookie) override {

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

template<class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };

//template<class Base, class... Ts> struct OverloadedFrom : Overloaded<Ts...> {
//
//	//OverloadedFrom(Ts... lambdas) : base(), Ts(std::move(lambdas)) ... {}
//
//	//OverloadedFrom(std::initializer_list<Ts ...> lambdas) : base(), Ts(std::move(lambdas)) ... {}
//
//
//	//using Overloaded<Ts...>::operator();
//
//	template<typename ... Args>
//	decltype(auto) operator()(Args&& ... args) {
//		return base(std::forward<Args>(args) ...);
//	}
//
//	Base base{};
//};
//
//template<typename Base, typename ... Ts>
//OverloadedFrom(Ts ...) -> OverloadedFrom<Base, Ts ...>;

struct RequestCheckerWrong {
	auto operator()(std::monostate) { return "wrong-monostate"; };
	auto operator()(ConfiguredGetRequest request) { return "wrong-get"; };
	auto operator()(ConfiguredPostRequest request) { return "wrong-post"; };
	auto operator()(ConfiguredPostMultipartRequest request) { return "wrong-multipart"; };
};

CORO_TEST_CASE("RequestorContext construct") {
	auto parser_config = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();
	auto client_logger = std::make_shared<DummyLogger>();

	auto mock_client = std::make_shared<ClientContextMock>(client_cookie_jar);

	REQUIRE_THROWS(RequestorContext(nullptr, nullptr, nullptr));
	REQUIRE_THROWS(RequestorContext(nullptr, client_logger, parser_config));
	REQUIRE_NOTHROW(RequestorContext(mock_client, nullptr, nullptr));
	REQUIRE_NOTHROW(RequestorContext(mock_client, client_logger, parser_config));

	RequestorContext context(mock_client, client_logger, parser_config);

	REQUIRE(context.config() == parser_config);

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
			"User-Agent: 123",
			"Authorization: some-password",
		},
	};

	auto parser_config = std::make_shared<ParserConfig>();
	auto client_cookie_jar = std::make_shared<DummyCookieJar>();

	auto mock_client = std::make_shared<ClientContextMock>(client_cookie_jar);

	RequestorContext context(mock_client, nullptr, parser_config);

	REQUIRE_THROWS_AS(co_await context.request(test_request), MockSuccess);

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
			"User-Agent: 123",
			"Authorization: some-password",
		},
	};

	GetRequest test_request = {
		.url = "/api/test",
		.url_params = {
			{ "token", "mytoken" },
		},
		.headers = {
			"User-Agent: 123",
		},
	};

	auto parser_config = std::make_shared<ParserConfig>();
	parser_config->headers["Authorization"] = "some-password";
	parser_config->url_params["version"] = "1.0";
	parser_config->modifiers.push_back([](ClientRequest& any_request) {
		if (auto* request = std::get_if<GetRequest>(&any_request)) {
			request->url.insert(0, "/v1");
			//request->headers["User-Agent"] += "/1.0";
		}
	});

	auto client_cookie_jar = std::make_shared<DummyCookieJar>();

	auto mock_client = std::make_shared<ClientContextMock>(client_cookie_jar);

	RequestorContext context(mock_client, nullptr, parser_config);

	REQUIRE_THROWS_AS(co_await context.request(test_request), MockSuccess);

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

	auto mock_client = std::make_shared<ClientContextMock>(client_cookie_jar);

	RequestorContext context(mock_client, nullptr, parser_config);

	auto check_request = [&]<typename ExpectedRequest>(const ExpectedRequest& expected_request) {
		if (auto* request = std::get_if<ConfiguredRequest<ExpectedRequest>>(&mock_client->request)) {
			return request->request == expected_request && request->cookies == client_cookie_jar;
		}
		return false;
	};

	REQUIRE_THROWS_AS(co_await context.request(test_get_request), MockSuccess);
	REQUIRE(check_request(test_get_request));

	REQUIRE_THROWS_AS(co_await context.request(test_post_request), MockSuccess);
	REQUIRE(check_request(test_post_request));

	REQUIRE_THROWS_AS(co_await context.request(test_post_multipart_request), MockSuccess);
	REQUIRE(check_request(test_post_multipart_request));
}

// TODO:
//  Test for correct response
//  Test for errors
//  Test for config priority