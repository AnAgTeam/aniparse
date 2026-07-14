/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <chrono>
#include <optional>
#include <string_view>

/**
 * @file
 * Reading the timestamps a source sends. Hand-written rather than delegated to
 * std::chrono::parse: that function is absent from Apple's libc++, so a parser that
 * used it would build on one platform and not the other. It is also the cheaper of
 * the two — no locale, no stream, no allocation.
 */

namespace aniparse {

/**
 * @brief Parse an ISO 8601 timestamp, as a JSON API spells one.
 *
 * Accepts what sources actually send:
 *  - a bare date, "2024-03-17" — midnight UTC;
 *  - a date and time, "2024-03-17T08:21:44", separated by 'T' or a space;
 *  - seconds omitted, "2024-03-17T08:21";
 *  - fractional seconds, "...:44.123456", kept to whatever precision system_clock has
 *    (a microsecond on libc++, 100 ns on MSVC) and truncated below it;
 *  - a zone: 'Z', "+03:00", "+0300", or none, in which case UTC is assumed.
 *
 * @param text The timestamp, with no surrounding whitespace.
 * @return The instant, or nullopt if @p text is not a timestamp. Note that nullopt is
 *         NOT @ref unknown_time: a caller that models "unknown" as the epoch maps it
 *         over itself, so that a source sending "1970-01-01" is not mistaken for a
 *         source sending nothing.
 */
[[nodiscard]] std::optional<std::chrono::system_clock::time_point> parse_iso8601(
    std::string_view text) noexcept;

} // namespace aniparse
