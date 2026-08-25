/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "catch_amalgamated.hpp"

#include <aniparse/utility/Number.hpp>

using aniparse::number::parse_decimal_prefix;

TEST_CASE("parse_decimal_prefix accepts source decimal prefixes", "[number]") {
	CHECK(parse_decimal_prefix("4.5 / 5") == 4.5);
	CHECK(parse_decimal_prefix("\t8,75 votes") == 8.75);
	CHECK(parse_decimal_prefix("12") == 12.0);
	CHECK(parse_decimal_prefix("0.0") == 0.0);
}

TEST_CASE("parse_decimal_prefix rejects absent numeric prefixes", "[number]") {
	CHECK_FALSE(parse_decimal_prefix("").has_value());
	CHECK_FALSE(parse_decimal_prefix("  no rating").has_value());
	CHECK_FALSE(parse_decimal_prefix(".").has_value());
}
