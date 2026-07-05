/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aniparse {
using pageoff = std::ptrdiff_t;
inline constexpr size_t page_no_limit = static_cast<size_t>(-1);

template <typename T>
struct PageItem {
	T item;
	pageoff offset;
};

template <typename T, typename Container = std::vector<PageItem<T>>>
struct PageResults {
	Container results;
	pageoff next_offset = pageoff(0);
	size_t total_count  = 0;
};

/**
 * @brief One ordering of paged results: an open key plus a direction.
 * Keys are open strings so a source can expose orderings the core does
 * not know about; common ones are named in sort_keys.
 * @see sort_keys
 */
struct SortOrder {
	std::string key;
	/// Most sources default to descending, so it is the default here too
	bool ascending = false;

	friend bool operator==(const SortOrder&, const SortOrder&) = default;
};

/**
 * @brief Declares which directions an endpoint supports for one sort key.
 */
struct SortDescriptor {
	bool ascending  = false;
	bool descending = true;
};

/// Sort declaration of one endpoint (search/latest/...), key -> directions
using SupportedSorts = std::map<std::string, SortDescriptor, std::less<>>;

struct GetFilters {
	/// 0-based ITEM offset to start from (not a page index). A page-based source
	/// maps it to page = from / page_size + 1 and skips from % page_size within
	/// that page; each returned PageItem carries its absolute offset.
	pageoff from = 0;
	/// Maximum number of items to return; page_no_limit for as many as available.
	size_t limit = page_no_limit;
	/// No value = the source's default order
	std::optional<SortOrder> sort;
};
} // namespace aniparse

/**
 * Namespace for default sort keys
 * @see SortOrder
 */
namespace aniparse::sort_keys {
inline constexpr std::string_view popularity   = "popularity";
inline constexpr std::string_view rating       = "rating";
inline constexpr std::string_view views        = "views";
inline constexpr std::string_view title        = "title";
inline constexpr std::string_view release_time = "rel_time";
inline constexpr std::string_view update_time  = "upd_time";
inline constexpr std::string_view downloads    = "downloads";
inline constexpr std::string_view author       = "author";

/// Popularity over a trailing window ("popular today"), usually descending-only
inline constexpr std::string_view popularity_hour  = "popularity_hour";
inline constexpr std::string_view popularity_day   = "popularity_day";
inline constexpr std::string_view popularity_week  = "popularity_week";
inline constexpr std::string_view popularity_month = "popularity_month";
inline constexpr std::string_view popularity_year  = "popularity_year";
} // namespace aniparse::sort_keys
