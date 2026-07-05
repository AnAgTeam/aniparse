/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/UrlView.hpp>

using namespace aniparse;

TEST_CASE("UrlView parses scheme, host and path", "[urlview]") {
	auto url = UrlView::parse("https://x8.h-chan.me/manga/12345-title.html?tab=info#top");
	REQUIRE(url.has_value());
	REQUIRE(url->scheme() == "https");
	REQUIRE(url->host() == "x8.h-chan.me");
	REQUIRE(url->path() == "/manga/12345-title.html");
	REQUIRE(url->query().find("tab=info") != std::string_view::npos);
}

TEST_CASE("UrlView exposes the path for routing", "[urlview]") {
	auto url = UrlView::parse("https://site.tld/anime/99");
	REQUIRE(url.has_value());
	REQUIRE(url->path().starts_with("/anime/"));
}

TEST_CASE("UrlView rejects a string without a scheme", "[urlview]") {
	REQUIRE_FALSE(UrlView::parse("not a url").has_value());
}
