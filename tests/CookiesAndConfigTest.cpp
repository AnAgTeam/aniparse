/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <aniparse/net/Client.hpp>

using namespace aniparse;

std::vector<std::string> test_curl_cookies = {
    ".example.com\tTRUE\t/\tFALSE\t0\tsession\tabc123",
    "example.com\tFALSE\t/\tFALSE\t0\tuser\talice",
    ".example.com\tTRUE\t/\tFALSE\t0\tpersistent\tvalue",
};

TEST_CASE("Cookies empty serialize") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	auto serialized_cookies = cookie_jar->serialize();
	REQUIRE(serialized_cookies.empty());
}

TEST_CASE("Cookies serialize/deserialize") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	cookie_jar->deserialize(test_curl_cookies);

	auto serialized_cookies = cookie_jar->serialize();

	REQUIRE(test_curl_cookies == serialized_cookies);
}

TEST_CASE("Cookies find_cookie returns value by name") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	cookie_jar->deserialize(test_curl_cookies);

	REQUIRE(cookie_jar->find_cookie("session")->value == "abc123");
	REQUIRE(cookie_jar->find_cookie("user")->value == "alice");
	REQUIRE(cookie_jar->find_cookie("persistent")->value == "value");
}

TEST_CASE("Cookies find_cookie parses metadata") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	cookie_jar->deserialize(test_curl_cookies);

	auto session = cookie_jar->find_cookie("session");
	REQUIRE(session.has_value());
	REQUIRE(session->domain == ".example.com");
	REQUIRE(session->include_subdomains == true);
	REQUIRE(session->path == "/");
	REQUIRE(session->secure == false);
	REQUIRE(session->expires == std::nullopt); // 0 => session cookie

	auto user = cookie_jar->find_cookie("user");
	REQUIRE(user.has_value());
	REQUIRE(user->domain == "example.com");
	REQUIRE(user->include_subdomains == false);
}

TEST_CASE("Cookies enumerates all") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	cookie_jar->deserialize(test_curl_cookies);

	REQUIRE(cookie_jar->cookies().size() == test_curl_cookies.size());
}

TEST_CASE("Cookies set_cookie round-trips through find_cookie") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	Cookie cookie{
	    .name               = "token",
	    .value              = "xyz789",
	    .domain             = "example.org",
	    .path               = "/",
	    .include_subdomains = false,
	};
	cookie_jar->set_cookie(cookie);

	auto found = cookie_jar->find_cookie("token");
	REQUIRE(found.has_value());
	REQUIRE(found->value == "xyz789");
	REQUIRE(found->domain == "example.org");
}

TEST_CASE("Cookies find_cookie missing returns nullopt") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	cookie_jar->deserialize(test_curl_cookies);

	REQUIRE(cookie_jar->find_cookie("nonexistent") == std::nullopt);
}

TEST_CASE("Cookies find_cookie on empty jar returns nullopt") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	REQUIRE(cookie_jar->find_cookie("session") == std::nullopt);
}

TEST_CASE("Cookies serialize and check empty") {
	AsyncClient client;
	auto cookie_jar = client.make_cookie_jar();

	cookie_jar->deserialize(test_curl_cookies);

	auto serialized_cookies = cookie_jar->serialize();
	REQUIRE(!serialized_cookies.empty());

	cookie_jar->clear();

	serialized_cookies = cookie_jar->serialize();
	REQUIRE(serialized_cookies.empty());
}