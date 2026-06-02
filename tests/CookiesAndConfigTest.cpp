/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <aniparse/Client.hpp>

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