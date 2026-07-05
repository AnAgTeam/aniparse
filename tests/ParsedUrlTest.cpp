/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/ParsedUrl.hpp>

using namespace aniparse;

TEST_CASE("ParsedUrl parses scheme, host and path", "[parsedurl]") {
	auto url = ParsedUrl::parse("https://cdn.example.com/manga/12345-title.html?tab=info#top");
	REQUIRE(url.has_value());
	REQUIRE(url->scheme() == "https");
	REQUIRE(url->host() == "cdn.example.com");
	REQUIRE(url->path() == "/manga/12345-title.html");
	REQUIRE(url->query().find("tab=info") != std::string_view::npos);
}

TEST_CASE("ParsedUrl exposes the path for routing", "[parsedurl]") {
	auto url = ParsedUrl::parse("https://site.tld/anime/99");
	REQUIRE(url.has_value());
	REQUIRE(url->path().starts_with("/anime/"));
}

TEST_CASE("ParsedUrl rejects a string without a scheme", "[parsedurl]") {
	REQUIRE_FALSE(ParsedUrl::parse("not a url").has_value());
}
