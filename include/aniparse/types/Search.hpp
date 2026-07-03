/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Pagination.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace aniparse {
/**
 * @brief One text input
 */
struct TextQuery {
	/**
	 * Search context: text value of an item
	 * Support check: Name of input
	 */
	std::string text;
	/**
	 * Search context: Is text inversed (i.e. include not containing text)
	 * Support check: Is inverse supported
	 */
	bool exclusive = false;
};

enum class TimeIntervalPrecision {
	Any,
	Now,
	Hour,
	Day,
	Week,
	Month,
	Year,
};

/**
 * @brief Basic interval [from, to]
 */
template <typename T>
struct Interval {
	/**
	 * Search context: No limit or start of interval
	 * Support check: Min value of interval
	 */
	std::optional<T> from;
	/**
	 * Search context: No limit or end of interval
	 * Support check: Max value of interval
	 */
	std::optional<T> to;
	/**
	 * Search context: Is interval exclusive (not containing in interval).
	 *                 For example exclusive interval [10, 20] => (-inf;10]U[20;+inf)
	 * Support check: Is exclusive supported
	 */
	bool exclusive = false;
};

/**
 * @brief Integer interval
 * Can be used for pages count, episodes count, etc.
 */
using IntInterval = Interval<std::ptrdiff_t>;

/**
 * @brief Time interval
 * Can be used for update time, etc.
 */
using TimeInterval = Interval<std::chrono::system_clock::time_point>;

/**
 * @brief Relative time interval
 * Counts time from now(). Can be used for update time, etc.
 */
using RelativeTimeInterval = Interval<std::chrono::system_clock::duration>;

struct ItemSelectionValue {
	std::string value;
	bool enabled;
	bool exclusive;
};

using ItemSelection = std::map<std::string, ItemSelectionValue, std::less<>>;

/**
 * @brief Switch/checkmark. If presented means true
 *        exclusion (if supported).
 */
struct Checkmark {
	bool exclusive = false;
};

/**
 * @brief Any type that can be used for search
 * @see SearchRequestQuery
 */
using SearchItemVariant = std::variant<
    TextQuery,
    IntInterval,
    TimeInterval,
    RelativeTimeInterval,
    ItemSelection,
    Checkmark>;

using SearchItems = std::map<std::string, SearchItemVariant, std::less<>>;

/**
 * @brief Any type that can be used for search
 * @see SearchItemVariant
 * @see search_keys
 */
struct SearchRequestQuery {
	std::string query;
	SearchItems filters;
};

/**
 * @brief One violation found by validate_search_query
 * @see validate_search_query
 */
struct SearchQueryError {
	enum class Reason {
		/// The filter key is not present in the supported set
		UnknownKey,
		/// The filter holds a different SearchItemVariant alternative than declared
		TypeMismatch,
		/// The filter requests exclusion, but the descriptor does not allow it
		ExclusionNotSupported,
		/// The value itself is malformed: empty text, inverted or out-of-bounds
		/// interval, selection of an undeclared item
		InvalidValue,
		/// The sort key is not present in the supported set
		UnknownSortKey,
		/// The sort key is supported, but not in the requested direction
		SortDirectionNotSupported,
	};

	Reason reason;
	/// Key of the offending entry in SearchRequestQuery::filters
	std::string key;

	friend bool operator==(const SearchQueryError&, const SearchQueryError&) = default;
};

/**
 * @brief Validate query filters against a declared support table.
 * Keeps supported_filters the single source of truth: parsers call it at the
 * start of search(), UI can call it pre-flight to highlight invalid inputs.
 * @param supported Declared filters (SearchCompatibilities::supported_filters)
 * @param query The query to check
 * @return Empty if the query is valid; otherwise all violations found
 */
[[nodiscard]] std::vector<SearchQueryError> validate_search_query(
    const SearchItems& supported,
    const SearchRequestQuery& query);

/**
 * @brief Validate a requested sort order against an endpoint's declaration.
 * Same contract as validate_search_query, for the GetFilters::sort channel:
 * no sort requested is always valid (the source's default order).
 * @param supported Declared sorts of the endpoint being called
 * @param sort The requested order (GetFilters::sort)
 * @return Empty if the sort is valid; otherwise the violation
 */
[[nodiscard]] std::vector<SearchQueryError> validate_sort(
    const SupportedSorts& supported,
    const std::optional<SortOrder>& sort);

/**
 * @brief Human-readable one-line summary of validation errors.
 * For the RequestError::message channel; UI should use the typed
 * errors from validate_search_query instead.
 */
[[nodiscard]] std::string describe_search_query_errors(std::span<const SearchQueryError> errors);
} // namespace aniparse

/**
 * Namespace for default keys, that can be used for search
 * @see SearchRequestQuery
 */
namespace aniparse::search_keys {
/// Filter by @see Series. Usually TextQuery
inline constexpr std::string_view series          = "series";
/// Filter by pages/episodes count. Usually DirectionalInterval/BidirectionalInterval
inline constexpr std::string_view pages           = "icount";
inline constexpr std::string_view episodes        = "icount";
/// Filter by Tag. Usually TextQuery
inline constexpr std::string_view tag             = "tag";
/// Filter by upload time (last time when item was updated on specific page). Usually DirectionalInterval/BidirectionalInterval
inline constexpr std::string_view upload_time     = "upd_time";
/// Filter by release time (actual time when item was released). Usually DirectionalInterval/BidirectionalInterval
inline constexpr std::string_view release_time    = "rel_time";
/// Filter by title, the text must be contained in title, but maybe not fully. Usually TextQuery
inline constexpr std::string_view title           = "title";
/// Filter by status like "Announced", "Released" (Aired state of item). Usually ? (TextQuery)
inline constexpr std::string_view status          = "status";
/// Filter by Rating. ? (DirectionalInterval/BidirectionalInterval)
inline constexpr std::string_view rating          = "rating";
/// Filter by year. ? (DirectionalInterval/BidirectionalInterval)
inline constexpr std::string_view year            = "year";
/// Filter by AgeRestriction. Usually DirectionalInterval/ItemSelection
inline constexpr std::string_view age_restriction = "age_res";
} // namespace aniparse::search_keys
