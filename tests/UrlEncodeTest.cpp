/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/utility/UrlEncode.hpp>
#include <aniparse/types/Request.hpp>

using namespace aniparse;

TEST_CASE("url_encode passes unreserved characters through", "[urlencode]") {
	REQUIRE(url_encode("abcXYZ019-._~") == "abcXYZ019-._~");
	REQUIRE(url_encode("").empty());
}

TEST_CASE("url_encode percent-encodes reserved characters and space", "[urlencode]") {
	REQUIRE(url_encode("a b&c=d") == "a%20b%26c%3Dd");
	REQUIRE(url_encode("/?#") == "%2F%3F%23");
}

TEST_CASE("url_encode percent-encodes UTF-8 bytes with uppercase hex", "[urlencode]") {
	// "Вход" is D0 92 D1 85 D0 BE D0 B4 in UTF-8.
	REQUIRE(url_encode("\xD0\x92\xD1\x85\xD0\xBE\xD0\xB4") == "%D0%92%D1%85%D0%BE%D0%B4");
}

TEST_CASE("to_urlencoded builds a form body from raw pairs", "[urlencode]") {
	UrlParameters form;
	form.add("login", "submit");
	form.add("login_name", "user name");
	form.add("login_password", "p&ss=1");

	REQUIRE(to_urlencoded(form) == "login=submit&login_name=user%20name&login_password=p%26ss%3D1");
}

TEST_CASE("to_urlencoded encodes keys too and yields empty for no params", "[urlencode]") {
	UrlParameters form;
	form.add("a b", "c");

	REQUIRE(to_urlencoded(form) == "a%20b=c");
	REQUIRE(to_urlencoded(UrlParameters{}).empty());
}
