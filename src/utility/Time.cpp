/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/utility/Time.hpp"

#include <algorithm>
#include <charconv>

namespace aniparse {
namespace {

/// Read exactly @p digits digits off the front of @p text and advance it. Nullopt if
/// they are not there — a short or non-numeric field is a malformed timestamp, not a
/// zero.
std::optional<int> take_digits(std::string_view& text, size_t digits) noexcept {
	if (text.size() < digits) {
		return std::nullopt;
	}
	// from_chars would accept a leading sign, and a timestamp field never carries one.
	if (text.front() == '+' || text.front() == '-') {
		return std::nullopt;
	}

	int value = 0;
	const char* const first  = text.data();
	const char* const last   = first + digits;
	const auto [stop, error] = std::from_chars(first, last, value);
	if (error != std::errc{} || stop != last) {
		return std::nullopt;
	}
	text.remove_prefix(digits);
	return value;
}

/// Consume @p expected if it is next. Reports whether it was.
bool take(std::string_view& text, char expected) noexcept {
	if (text.empty() || text.front() != expected) {
		return false;
	}
	text.remove_prefix(1);
	return true;
}

/// The date, or nullopt if what is at the front of @p text is not one.
std::optional<std::chrono::sys_days> take_date(std::string_view& text) noexcept {
	using namespace std::chrono;

	const auto year = take_digits(text, 4);
	if (!year || !take(text, '-')) {
		return std::nullopt;
	}
	const auto month = take_digits(text, 2);
	if (!month || !take(text, '-')) {
		return std::nullopt;
	}
	const auto day = take_digits(text, 2);
	if (!day) {
		return std::nullopt;
	}

	const year_month_day date{ std::chrono::year{ *year },
	                           std::chrono::month{ static_cast<unsigned>(*month) },
	                           std::chrono::day{ static_cast<unsigned>(*day) } };
	if (!date.ok()) {
		return std::nullopt;
	}
	return sys_days{ date };
}

/// The time of day, seconds and fraction included. Nullopt on a malformed one.
std::optional<std::chrono::system_clock::duration> take_time(std::string_view& text) noexcept {
	using namespace std::chrono;

	const auto hour = take_digits(text, 2);
	if (!hour || !take(text, ':')) {
		return std::nullopt;
	}
	const auto minute = take_digits(text, 2);
	if (!minute) {
		return std::nullopt;
	}

	// Seconds are optional: "2024-03-17T08:21" is a legal timestamp.
	int second = 0;
	if (take(text, ':')) {
		const auto parsed = take_digits(text, 2);
		if (!parsed) {
			return std::nullopt;
		}
		second = *parsed;
	}
	// 60 is a leap second: legal to send, and there is no such instant to name, so it
	// lands on :59 rather than rolling the minute over.
	if (*hour > 23 || *minute > 59 || second > 60) {
		return std::nullopt;
	}

	auto time = hours{ *hour } + minutes{ *minute } + seconds{ std::min(second, 59) };
	auto fraction = system_clock::duration::zero();

	if (take(text, '.') || take(text, ',')) {
		// Ticks per second — a microsecond on libc++, 100 ns on MSVC.
		auto scale = seconds{ 1 } / system_clock::duration{ 1 };
		bool any   = false;
		while (!text.empty() && text.front() >= '0' && text.front() <= '9') {
			const auto digit = static_cast<decltype(scale)>(text.front() - '0');
			text.remove_prefix(1);
			any = true;
			scale /= 10;
			// Digits past what the clock can hold are read and dropped, not rejected:
			// a source is free to send nanoseconds at us.
			if (scale > 0) {
				fraction += system_clock::duration{ digit * scale };
			}
		}
		if (!any) {
			return std::nullopt;
		}
	}

	return duration_cast<system_clock::duration>(time) + fraction;
}

/// How far the stamp's clock runs ahead of UTC. Zero when it says 'Z' or says nothing.
/// Nullopt if what follows is not a zone.
std::optional<std::chrono::minutes> take_zone(std::string_view& text) noexcept {
	using namespace std::chrono;

	if (text.empty()) {
		// No zone at all. A source sending naive local time is asserting UTC whether it
		// means to or not, and guessing a zone for it would be worse than taking it at
		// its word.
		return minutes{ 0 };
	}
	if (take(text, 'Z') || take(text, 'z')) {
		return text.empty() ? std::optional{ minutes{ 0 } } : std::nullopt;
	}

	const bool behind = text.front() == '-';
	if (!take(text, '+') && !take(text, '-')) {
		return std::nullopt;
	}
	const auto offset_hours = take_digits(text, 2);
	if (!offset_hours) {
		return std::nullopt;
	}

	int offset_minutes = 0;
	if (!text.empty()) {
		(void)take(text, ':');  // "+03:00" and "+0300" are the same offset
		const auto parsed = take_digits(text, 2);
		if (!parsed) {
			return std::nullopt;
		}
		offset_minutes = *parsed;
	}
	if (!text.empty() || *offset_hours > 23 || offset_minutes > 59) {
		return std::nullopt;
	}

	const auto offset = hours{ *offset_hours } + minutes{ offset_minutes };
	return behind ? -offset : offset;
}

} // namespace

std::optional<std::chrono::system_clock::time_point> parse_iso8601(std::string_view text) noexcept {
	using namespace std::chrono;

	const auto date = take_date(text);
	if (!date) {
		return std::nullopt;
	}

	// A bare date is a whole timestamp: midnight UTC.
	if (text.empty()) {
		return time_point_cast<system_clock::duration>(*date);
	}
	if (!take(text, 'T') && !take(text, ' ')) {
		return std::nullopt;
	}

	const auto time = take_time(text);
	if (!time) {
		return std::nullopt;
	}
	const auto zone = take_zone(text);
	if (!zone) {
		return std::nullopt;
	}

	// The zone says how far ahead of UTC the stamp's clock reads, so getting back to UTC
	// subtracts it.
	return time_point_cast<system_clock::duration>(*date) + *time - *zone;
}

} // namespace aniparse
