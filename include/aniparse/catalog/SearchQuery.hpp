/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace aniparse {
/// Precision vocabulary for time-oriented search controls.
enum class TimeIntervalPrecision { Any, Now, Hour, Day, Week, Month, Year };

/**
 * @brief A text filter value, independent of its UI and support declaration.
 */
struct TextQuery {
    std::string text; ///< Text supplied by the caller.
    bool exclusive = false; ///< Exclude matching works when true.
};

/**
 * @brief An inclusive interval chosen by a caller.
 * @tparam T The ordered endpoint type.
 */
template <typename T>
struct Interval {
    std::optional<T> from; ///< No value means no requested lower bound.
    std::optional<T> to; ///< No value means no requested upper bound.
    bool exclusive = false; ///< Select outside the interval when true.
};

/// An integer filter value, such as episode count or release year.
using IntInterval = Interval<std::ptrdiff_t>;
/// An absolute time filter value.
using TimeInterval = Interval<std::chrono::system_clock::time_point>;
/// A time filter expressed relative to the request time.
using RelativeTimeInterval = Interval<std::chrono::system_clock::duration>;

/**
 * @brief Opaque selected tokens, with no labels or capability metadata.
 *
 * Tokens are parser-owned identities, such as Tag::ref. Each token must be
 * nonempty and occur at most once across the two lists. An empty value makes
 * no selection. Labels belong to SelectionCapabilities, not to this value.
 */
struct SelectionValue {
    std::vector<std::string> included; ///< Tokens to include.
    std::vector<std::string> excluded; ///< Tokens to exclude.

    /**
     * @brief Count included and excluded tokens.
     * @return Total number of selected tokens.
     */
    std::size_t size() const noexcept { return included.size() + excluded.size(); }
    /**
     * @brief Check whether the caller selected no tokens.
     * @return True when both token lists are empty.
     */
    bool empty() const noexcept { return included.empty() && excluded.empty(); }
};

/**
 * @brief A present boolean filter, optionally inverted by the caller.
 */
struct Checkmark {
    bool exclusive = false; ///< Require the opposite state when true.
};

/// The value of one requested filter, with no support or presentation metadata.
using SearchFilterValue = std::variant<TextQuery, IntInterval, TimeInterval,
    RelativeTimeInterval, SelectionValue, Checkmark>;
/// Requested values keyed by stable parser-owned filter keys.
using SearchFilters = std::map<std::string, SearchFilterValue, std::less<>>;

/**
 * @brief Caller-selected search text and structured filter values.
 */
struct SearchRequestQuery {
    std::string query; ///< Main search text.
    SearchFilters filters; ///< Only filters selected by the caller.
};
} // namespace aniparse
