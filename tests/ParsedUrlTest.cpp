/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <utility>

#include <aniparse/types/ParsedUrl.hpp>

using namespace aniparse;

TEST_CASE("ParsedUrl parses scheme, host and path", "[parsedurl]") {
	auto url = ParsedUrl::parse("https://cdn.example.com/manga/12345-title.html?tab=info#top");
	REQUIRE(url.has_value());
	REQUIRE(url->scheme() == "https");
	REQUIRE(url->host() == "cdn.example.com");
	REQUIRE(url->path() == "/manga/12345-title.html");
	REQUIRE(url->query() == "tab=info");
	REQUIRE(url->fragment() == "top");
	REQUIRE(url->href() == "https://cdn.example.com/manga/12345-title.html?tab=info#top");
}

TEST_CASE("ParsedUrl keeps components after copying and moving", "[parsedurl]") {
	auto parsed = ParsedUrl::parse("https://example.com/anime/99?source=test#episode-1");
	REQUIRE(parsed.has_value());

	auto copy = *parsed;
	auto moved = std::move(copy);

	REQUIRE(moved.href() == "https://example.com/anime/99?source=test#episode-1");
	REQUIRE(moved.host() == "example.com");
	REQUIRE(moved.path() == "/anime/99");
	REQUIRE(moved.query() == "source=test");
	REQUIRE(moved.fragment() == "episode-1");
}

TEST_CASE("ParsedUrl href preserves credentials and port", "[parsedurl]") {
	auto url = ParsedUrl::parse("https://user:password@example.com:8443/anime/99");
	REQUIRE(url.has_value());
	REQUIRE(url->href() == "https://user:password@example.com:8443/anime/99");
	REQUIRE(url->host() == "example.com");
}

TEST_CASE("ParsedUrl exposes the path for routing", "[parsedurl]") {
	auto url = ParsedUrl::parse("https://site.tld/anime/99");
	REQUIRE(url.has_value());
	REQUIRE(url->path().starts_with("/anime/"));
}

TEST_CASE("ParsedUrl rejects a string without a scheme", "[parsedurl]") {
	REQUIRE_FALSE(ParsedUrl::parse("not a url").has_value());
}
