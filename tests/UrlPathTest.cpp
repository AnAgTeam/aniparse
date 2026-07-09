/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/utility/UrlPath.hpp>

using namespace aniparse;

TEST_CASE("all_digits accepts digit runs and rejects the rest", "[urlpath]") {
	REQUIRE(all_digits("12345"));
	REQUIRE(all_digits("0"));
	REQUIRE_FALSE(all_digits(""));
	REQUIRE_FALSE(all_digits("one-piece"));
	REQUIRE_FALSE(all_digits("12a"));
	REQUIRE_FALSE(all_digits(" 12"));
}

TEST_CASE("segment_after returns the slice up to a delimiter", "[urlpath]") {
	REQUIRE(segment_after("/manga/one-piece", "/manga/") == "one-piece");
	REQUIRE(segment_after("/manga/one-piece?tab=info", "/manga/") == "one-piece");
	REQUIRE(segment_after("/manga/38/chapters", "/manga/") == "38");
	REQUIRE(segment_after("/manga/slug#top", "/manga/") == "slug");
}

TEST_CASE("segment_after fails on a missing marker or empty segment", "[urlpath]") {
	REQUIRE_FALSE(segment_after("/anime/naruto", "/manga/").has_value());
	REQUIRE_FALSE(segment_after("/manga/", "/manga/").has_value());
	REQUIRE_FALSE(segment_after("/manga/?x=1", "/manga/").has_value());
}

TEST_CASE("segment_after owns its result past the input", "[urlpath]") {
	// The result must survive a temporary input (owning string, not a view).
	std::optional<std::string> segment;
	{
		const std::string path = "/manga/scoped";
		segment = segment_after(path, "/manga/");
	}
	REQUIRE(segment.has_value());
	REQUIRE(*segment == "scoped");
}

TEST_CASE("numeric_after reads the digit run after a marker", "[urlpath]") {
	REQUIRE(numeric_after("/posts/12345.json", "/posts/") == 12345);
	REQUIRE(numeric_after("/pools/7", "/pools/") == 7);
	REQUIRE(numeric_after("/manga/999/", "/manga/") == 999);
}

TEST_CASE("numeric_after fails without a trailing digit run", "[urlpath]") {
	REQUIRE_FALSE(numeric_after("/posts/abc", "/posts/").has_value());
	REQUIRE_FALSE(numeric_after("/posts/", "/posts/").has_value());
	REQUIRE_FALSE(numeric_after("/other/12", "/posts/").has_value());
}

TEST_CASE("numeric_after with a boundary skips substring matches", "[urlpath]") {
	// "id=" inside "pool_id=" / "parent_id=" must not match; the real key does.
	REQUIRE(numeric_after("?pool_id=5&id=42", "id=", /*require_boundary=*/true) == 42);
	REQUIRE(numeric_after("?parent_id=9", "id=", /*require_boundary=*/true) == std::nullopt);
	REQUIRE(numeric_after("id=7", "id=", /*require_boundary=*/true) == 7);
	REQUIRE(numeric_after("?id=3", "id=", /*require_boundary=*/true) == 3);
}

TEST_CASE("numeric_after without a boundary takes the first match", "[urlpath]") {
	// A page/query where the first occurrence is the intended one.
	REQUIRE(numeric_after("?id=8&pool_id=5", "id=") == 8);
}
