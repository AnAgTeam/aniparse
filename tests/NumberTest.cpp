#include <ranges>
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

TEST_CASE("Whole-number parsing separates invalid text from zero", "[number]") {
    using aniparse::number::parse;
    using aniparse::number::parse_or;
    CHECK(parse<int>("42") == 42);
    CHECK(parse<int>("-42") == -42);
    CHECK(parse<double>("9.78") == 9.78);
    CHECK(parse<double>("4.2e2") == 420);
    CHECK(parse_or<int>("0", 7) == 0);
    CHECK(parse_or<int>("42votes", 7) == 7);
    CHECK_FALSE(parse<unsigned>("-1"));
    for (auto text : {"", "42votes", "42.9", " 42", "42 ", "+42", "2147483648"}) {
        CHECK_FALSE(parse<int>(text));
    }
    for (auto text : {"", "nan", "inf", "1e9999", "9.78votes", " 9.78", "+9.78"}) {
        CHECK_FALSE(parse<double>(text));
    }
    const std::string text = "42votes";
    const auto prefix = text | std::views::take_while([](char c) { return c >= '0' && c <= '9'; });
    std::string digits;
    for (char c : prefix) digits.push_back(c);
    CHECK(parse_or<int>(digits, 0) == 42);
}

TEST_CASE("Integral decimal prefixes preserve precision and check destination bounds", "[number]") {
    CHECK(parse_decimal_prefix<std::uint64_t>("9007199254740993--title") == UINT64_C(9007199254740993));
    CHECK(parse_decimal_prefix<std::uint64_t>("18446744073709551615votes") == UINT64_MAX);
    CHECK_FALSE(parse_decimal_prefix<std::uint64_t>("18446744073709551616votes"));
    CHECK(parse_decimal_prefix<std::int64_t>("-9223372036854775808 suffix") == INT64_MIN);
    CHECK_FALSE(parse_decimal_prefix<std::int64_t>("-9223372036854775809 suffix"));
    CHECK_FALSE(parse_decimal_prefix<std::int64_t>("9223372036854775808 suffix"));
    CHECK(parse_decimal_prefix<int>(" \t\r\n7.5") == 7);
    CHECK(parse_decimal_prefix<int>("-7,5") == -7);
    CHECK(parse_decimal_prefix<int>("42votes") == 42);
    CHECK(parse_decimal_prefix<int>("0") == 0);
    CHECK(parse_decimal_prefix<std::uint8_t>("255+") == 255);
    CHECK_FALSE(parse_decimal_prefix<std::uint8_t>("256+"));
    CHECK_FALSE(parse_decimal_prefix<unsigned>("-1"));
    for (const auto text : {"", " ", "-", "+1", ".5", ",5", "words42"}) {
        CAPTURE(text);
        CHECK_FALSE(parse_decimal_prefix<int>(text));
    }
}

TEST_CASE("Floating decimal prefixes have a decimal grammar and finite typed results", "[number]") {
    CHECK(parse_decimal_prefix<float>("8,75 votes") == 8.75f);
    CHECK(parse_decimal_prefix<double>("-.5 / 5") == -0.5);
    CHECK(parse_decimal_prefix<long double>("12.5") == 12.5L);
    CHECK(parse_decimal_prefix<float>(".5") == 0.5f);
    CHECK(parse_decimal_prefix<double>("1.2.3") == 1.2);
    CHECK(parse_decimal_prefix<double>("1,2,3") == 1.2);
    CHECK(parse_decimal_prefix<double>("1e3") == 1.0);
    CHECK(parse_decimal_prefix<double>("1.") == 1.0);
    CHECK(parse_decimal_prefix<double>("1,") == 1.0);
    CHECK_FALSE(parse_decimal_prefix<float>(std::string(40, '9')));
    CHECK_FALSE(parse_decimal_prefix<double>(std::string(400, '9')));
    // Long fractional runs must not turn the fraction/scale accumulators into NaN.
    CHECK(parse_decimal_prefix<double>("0." + std::string(400, '0') + "1") == std::nullopt);
    CHECK(parse_decimal_prefix<double>("1." + std::string(400, '9')) == 2.0);
    for (const auto text : {"", "-", "-.", "+1", "nan", "inf", ",", "  words"}) {
        CAPTURE(text);
        CHECK_FALSE(parse_decimal_prefix<double>(text));
    }
}
