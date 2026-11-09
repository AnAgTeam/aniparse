/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include <optional>
#include <charconv>
#include <cmath>
#include <concepts>
#include <boost/charconv/from_chars.hpp>
#include <string>
#include <string_view>

namespace aniparse::number {
/// Arithmetic destinations for text parsing; bool is deliberately excluded.
template<class T>
concept ParseNumber = (std::integral<T> && !std::same_as<T, bool>) || std::floating_point<T>;

/**
 * @brief Parse an entire decimal integer or general-format floating-point string.
 * @param text Text to parse, without whitespace or a leading plus sign.
 * @return The number, or nullopt for empty input, trailing characters, overflow,
 *         invalid syntax or a nonfinite result. No prefix parsing or truncation.
 */
template<ParseNumber T>
std::optional<T> parse(std::string_view text) noexcept {
    if (text.empty()) return std::nullopt;
    T value{};
    if constexpr (std::floating_point<T>) {
        const auto result = boost::charconv::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
            || !std::isfinite(value)) return std::nullopt;
    } else {
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return std::nullopt;
    }
    return value;
}

/**
 * @brief Parse a whole numeric string with an explicit fallback.
 * @param text Text accepted by parse.
 * @param fallback Value returned if parsing fails.
 * @return Parsed number, or fallback.
 */
template<ParseNumber T>
T parse_or(std::string_view text, T fallback) noexcept {
    return parse<T>(text).value_or(fallback);
}

namespace detail {
// Strip the same leading ASCII whitespace accepted by source-number parsing.
std::string_view trim_numeric_prefix(std::string_view text) noexcept;
} // namespace detail

/**
 * @brief Parse a decimal integer prefix directly into the destination type.
 * @tparam T Integral destination other than bool.
 * @param text Text with optional leading ASCII whitespace and a decimal integer.
 *        A minus sign is accepted for signed destinations; plus is rejected.
 * @return The integer, or nullopt for missing digits or destination overflow.
 *         Parsing stops at the first nondigit: "7.5" yields 7, ".5" has no
 *         integer prefix, and "42votes" yields 42. No floating-point conversion.
 */
template<std::integral T>
    requires (!std::same_as<T, bool>)
std::optional<T> parse_decimal_prefix(std::string_view text) noexcept {
    text = detail::trim_numeric_prefix(text);
    if (text.empty()) return std::nullopt;
    T value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{}) return std::nullopt;
    return value;
}

/**
 * @brief Parse a decimal floating-point prefix without locale-sensitive APIs.
 * @tparam T Floating-point destination; double when omitted.
 * @param text Text with optional leading ASCII whitespace, an optional minus,
 *        digits and one decimal separator (dot or comma). Plus is rejected.
 * @return A finite number representable by T, or nullopt for absent digits or
 *         range errors. Trailing labels are ignored; exponents are not part of
 *         this decimal-prefix grammar ("4.5 / 5" yields 4.5, "1e3" yields 1).
 * @note A comma prefix is copied for normalization; dot prefixes are borrowed
 *       only during this call. The returned number owns no references.
 */
template<std::floating_point T = double>
std::optional<T> parse_decimal_prefix(std::string_view text) {
    text = detail::trim_numeric_prefix(text);
    std::size_t end = !text.empty() && text.front() == '-' ? 1 : 0;
    std::size_t separator = std::string_view::npos;
    while (end < text.size()) {
        const char character = text[end];
        if (character >= '0' && character <= '9') {
            ++end;
        } else if (separator == std::string_view::npos && (character == '.' || character == ',')) {
            separator = end++;
        } else {
            break;
        }
    }
    const auto prefix = text.substr(0, end);
    if (separator != std::string_view::npos && text[separator] == ',') {
        std::string normalized(prefix);
        normalized[separator] = '.';
        return parse<T>(normalized);
    }
    return parse<T>(prefix);
}
} // namespace aniparse::number
