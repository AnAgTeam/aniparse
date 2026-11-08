/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/source/Capabilities.hpp"
#include "aniparse/catalog/Pagination.hpp"

#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace aniparse {
/**
 * @brief The supported operations for a text filter.
 */
struct TextCapabilities {
    bool supports_exclusion = false; ///< Whether callers may invert the text filter.
};

/**
 * @brief Allowed endpoint bounds and operations for an interval filter.
 * @tparam T The ordered endpoint type.
 */
template <typename T>
struct IntervalCapabilities {
    std::optional<T> min; ///< Minimum allowed endpoint; absent means unrestricted.
    std::optional<T> max; ///< Maximum allowed endpoint; absent means unrestricted.
    bool supports_exclusion = false; ///< Whether the interval can be inverted.
    /// Optional positive, finite UI increment; absent lets the consumer choose.
    /// This is a distance (a duration for time points), not a required value grid.
    /// validate_query never checks multiples or rounds values. Parsers enforce any
    /// stricter source constraints themselves. Consumers ignore invalid hints.
    std::optional<decltype(T{} - T{})> step;
};

/// Capabilities of an integer interval filter.
using IntIntervalCapabilities = IntervalCapabilities<std::ptrdiff_t>;
/// Capabilities of a finite floating-point interval filter.
using DoubleIntervalCapabilities = IntervalCapabilities<double>;
/// Capabilities of an absolute time interval filter.
using TimeIntervalCapabilities = IntervalCapabilities<std::chrono::system_clock::time_point>;
/// Capabilities of a relative time interval filter.
using RelativeTimeIntervalCapabilities = IntervalCapabilities<std::chrono::system_clock::duration>;

/**
 * @brief Presentation and optional operation override for a known selection token.
 */
struct SelectionOption {
    std::string name; ///< Display label; the option map key is the opaque token.
    std::optional<bool> supports_exclusion; ///< Override the axis default for this token.
};

/**
 * @brief Offered options and permitted operations for a token-selection filter.
 *
 * Options and vocabulary openness are independent: an open axis may offer known
 * options and still accept custom tokens. A closed axis accepts only listed
 * tokens, including when its option map is empty. Per-option exclusion overrides
 * apply to known tokens on both open and closed axes.
 */
struct SelectionCapabilities {
    std::map<std::string, SelectionOption, std::less<>> options; ///< Known UI choices.
    bool open_vocabulary = false; ///< Accept tokens absent from options.
    bool supports_exclusion = false; ///< Default exclusion support, including custom tokens.
    bool multiple = true; ///< Allow more than one token across included and excluded lists.
};

/**
 * @brief Supported operations for a boolean filter.
 */
struct CheckmarkCapabilities {
    bool supports_exclusion = false; ///< Whether the flag can require the opposite state.
};

/// Description of one supported filter, separate from request values.
using SearchFilterCapabilities = std::variant<TextCapabilities, IntIntervalCapabilities,
    TimeIntervalCapabilities, RelativeTimeIntervalCapabilities, SelectionCapabilities,
    CheckmarkCapabilities, DoubleIntervalCapabilities>;
/**
 * @brief Display metadata and supported operations for one filter axis.
 *
 * The enclosing map key is the stable identity sent in queries. The name is
 * presentation only, in the source's language. Sources must provide a nonempty
 * name for custom keys; canonical keys may omit it. Consumers prefer their own
 * localized title for canonical keys and use this name for custom filters.
 */
struct SearchFilterInfo {
    std::string name; ///< Source-provided axis title, independent of option names.
    SearchFilterCapabilities capabilities; ///< Accepted values and operations.
};

/// Supported filters keyed by stable parser-owned filter keys.
using SupportedSearchFilters = std::map<std::string, SearchFilterInfo, std::less<>>;

/**
 * @brief Search filters, ordering and autocomplete capabilities of an endpoint.
 */
struct SearchCapabilities {
    SupportedSearchFilters supported_filters; ///< Accepted filter types and constraints.
    SupportedSorts supported_sorts; ///< Accepted sort keys and directions.
    CapabilitiesFlags capabilities = capabilities_flags::default_flags; ///< Search feature flags.
    std::vector<std::string> supported_suggestion_kinds; ///< Accepted suggestion axes; empty means the default axis.
};
} // namespace aniparse
