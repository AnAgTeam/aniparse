/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include <optional>
#include <string_view>

namespace aniparse::number {
/**
 * @brief Parse the unsigned decimal prefix of text without locale-sensitive APIs.
 *
 * Accepts a leading ASCII-space run, digits, and one decimal separator (`.` or
 * `,`). Parsing stops at the first other character, so source labels such as
 * @c "4.5 / 5" remain usable. Returns no value when the prefix contains no digit.
 *
 * @param text Text beginning with a decimal value.
 * @return Parsed value, or std::nullopt when no decimal prefix exists.
 */
std::optional<double> parse_decimal_prefix(std::string_view text) noexcept;
} // namespace aniparse::number
