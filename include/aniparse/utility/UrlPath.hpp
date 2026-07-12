/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/**
 * @file
 * Small scanners for pulling an id or a slug out of a URL path or query, so
 * parse_url does not become a hand-rolled character loop in every parser:
 * is this ref numeric, what follows this marker, what number follows it.
 */

namespace aniparse {

/**
 * @brief True when @p text is non-empty and every byte is an ASCII digit.
 * Handy for deciding whether a URL ref is a numeric id or an opaque slug.
 */
[[nodiscard]] inline bool all_digits(std::string_view text) noexcept {
	if (text.empty()) {
		return false;
	}
	for (char c : text) {
		if (c < '0' || c > '9') {
			return false;
		}
	}
	return true;
}

/**
 * @brief The slice of @p text right after the first @p marker, up to the first
 *        delimiter (or end of string), copied into an owning string.
 *
 * Extracts a path/query segment without a hand-rolled scan, e.g.
 * `segment_after("/manga/one-piece?tab=info", "/manga/")` -> `"one-piece"`.
 * Returns an owning string rather than a view: the result outlives @p text, so a
 * caller extracting a ref from a temporary path cannot be left with a dangle.
 * @param text   The string to search (a path or query view).
 * @param marker The substring the segment follows; the first occurrence wins.
 * @param delims Characters that terminate the segment (default: path/query/fragment
 *               separators).
 * @return The segment, or nullopt if @p marker is absent or the segment is empty.
 */
[[nodiscard]] inline std::optional<std::string> segment_after(
    std::string_view text, std::string_view marker,
    std::string_view delims = "/?#") {
	const std::size_t pos = text.find(marker);
	if (pos == std::string_view::npos) {
		return std::nullopt;
	}
	const std::size_t start = pos + marker.size();
	std::size_t end = start;
	while (end < text.size() && delims.find(text[end]) == std::string_view::npos) {
		++end;
	}
	if (end == start) {
		return std::nullopt;
	}
	return std::string(text.substr(start, end - start));
}

/**
 * @brief The run of ASCII digits immediately following @p marker, as an integer.
 *
 * Replaces the per-parser "find marker, scan digits, atol" loop, e.g.
 * `numeric_after("/posts/12345.json", "/posts/")` -> `12345`.
 * @param text             The string to search (a path or query view).
 * @param marker           The substring the digits follow.
 * @param require_boundary When set, @p marker must sit at the start of @p text or
 *                         right after '&' or '?', so a query key like `id=` does not
 *                         match inside `pool_id=`; occurrences are scanned until one
 *                         both satisfies the boundary and is followed by digits.
 * @return The parsed value, or nullopt if no qualifying digit run exists.
 */
[[nodiscard]] inline std::optional<std::int64_t> numeric_after(
    std::string_view text, std::string_view marker, bool require_boundary = false) {
	constexpr std::string_view boundary = "&?";
	std::size_t pos = text.find(marker);
	while (pos != std::string_view::npos) {
		const bool ok_boundary = !require_boundary || pos == 0
		                       || boundary.find(text[pos - 1]) != std::string_view::npos;
		if (ok_boundary) {
			const std::size_t start = pos + marker.size();
			std::size_t end = start;
			while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
				++end;
			}
			if (end > start) {
				std::int64_t value = 0;
				for (std::size_t i = start; i < end; ++i) {
					// Widen before subtracting so the digit math is 64-bit (C26451).
					value = value * 10 + (static_cast<std::int64_t>(text[i]) - '0');
				}
				return value;
			}
		}
		pos = text.find(marker, pos + marker.size());
	}
	return std::nullopt;
}

} // namespace aniparse
