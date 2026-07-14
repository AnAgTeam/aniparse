/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <aniparse/utility/Time.hpp>

using namespace aniparse;
using namespace std::chrono;

namespace {

/// The instant a stamp names, spelled out — so a test says what it expects rather than
/// restating the parser's own arithmetic back at it.
system_clock::time_point utc(int year, unsigned month, unsigned day,
                             int hour = 0, int minute = 0, int second = 0) {
	const auto date = sys_days{ std::chrono::year{ year } / std::chrono::month{ month }
	                            / std::chrono::day{ day } };
	return time_point_cast<system_clock::duration>(
	    date + hours{ hour } + minutes{ minute } + seconds{ second });
}

} // namespace

TEST_CASE("parse_iso8601 reads the forms a source sends") {
	CHECK(parse_iso8601("2024-03-17T08:21:44") == utc(2024, 3, 17, 8, 21, 44));
	CHECK(parse_iso8601("2024-03-17 08:21:44") == utc(2024, 3, 17, 8, 21, 44));
	CHECK(parse_iso8601("2024-03-17T08:21:44Z") == utc(2024, 3, 17, 8, 21, 44));

	// A bare date is midnight UTC.
	CHECK(parse_iso8601("2024-03-17") == utc(2024, 3, 17));
	// Seconds may be left off.
	CHECK(parse_iso8601("2024-03-17T08:21") == utc(2024, 3, 17, 8, 21));
}

TEST_CASE("parse_iso8601 applies the zone offset") {
	// The offset says how far AHEAD of UTC the stamp's clock reads, so 08:21 in +03:00
	// is 05:21 UTC. Getting the sign backwards is the classic bug here.
	CHECK(parse_iso8601("2024-03-17T08:21:44+03:00") == utc(2024, 3, 17, 5, 21, 44));
	CHECK(parse_iso8601("2024-03-17T08:21:44-05:00") == utc(2024, 3, 17, 13, 21, 44));

	// The colon is optional, and a half-hour zone is a real zone (India, +05:30).
	CHECK(parse_iso8601("2024-03-17T08:21:44+0300") == utc(2024, 3, 17, 5, 21, 44));
	CHECK(parse_iso8601("2024-03-17T00:00:00+05:30") == utc(2024, 3, 16, 18, 30, 0));
}

TEST_CASE("parse_iso8601 keeps the fraction the clock can hold") {
	const auto parsed = parse_iso8601("2024-03-17T08:21:44.500000");
	REQUIRE(parsed.has_value());
	CHECK(*parsed == utc(2024, 3, 17, 8, 21, 44) + milliseconds{ 500 });

	// Digits past the clock's precision are dropped, not rejected: a source is free to
	// send nanoseconds, and the alternative is failing to read a valid timestamp.
	const auto nanos = parse_iso8601("2024-03-17T08:21:44.123456789Z");
	REQUIRE(nanos.has_value());
	CHECK(*nanos >= utc(2024, 3, 17, 8, 21, 44) + microseconds{ 123456 });
	CHECK(*nanos < utc(2024, 3, 17, 8, 21, 44) + microseconds{ 123457 });
}

TEST_CASE("parse_iso8601 rejects what is not a timestamp") {
	CHECK_FALSE(parse_iso8601("").has_value());
	CHECK_FALSE(parse_iso8601("yesterday").has_value());
	CHECK_FALSE(parse_iso8601("2024").has_value());
	CHECK_FALSE(parse_iso8601("2024-03").has_value());

	// A calendar that does not exist.
	CHECK_FALSE(parse_iso8601("2023-02-29").has_value());
	CHECK_FALSE(parse_iso8601("2024-13-01").has_value());
	CHECK_FALSE(parse_iso8601("2024-03-17T25:00:00").has_value());

	// Trailing junk is a malformed stamp, not a stamp with a suffix — accepting it would
	// let "2024-03-17T08:21:44+0300 (MSK)" parse as UTC and be silently three hours off.
	CHECK_FALSE(parse_iso8601("2024-03-17T08:21:44 (MSK)").has_value());
	CHECK_FALSE(parse_iso8601("2024-03-17T08:21:44Zulu").has_value());
	CHECK_FALSE(parse_iso8601("2024-03-17T08:21:44.").has_value());

	// A leap year IS a leap year.
	CHECK(parse_iso8601("2024-02-29") == utc(2024, 2, 29));
}

TEST_CASE("parse_iso8601 does not confuse absent with the epoch") {
	// unknown_time is the epoch, so a source that genuinely sends 1970 must not be read
	// as a source that sent nothing. That distinction is the caller's to make, which it
	// can only do if the parser hands back an empty optional rather than a zero.
	const auto epoch = parse_iso8601("1970-01-01T00:00:00Z");
	REQUIRE(epoch.has_value());
	CHECK(*epoch == system_clock::time_point{});
	CHECK_FALSE(parse_iso8601("not a date").has_value());
}
