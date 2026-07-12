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

/**
 * @file
 * Paging and sort ordering, in ITEM offsets rather than page numbers: the caller
 * asks for a window of items, and OffsetPaging does the arithmetic that maps it
 * onto whatever the endpoint actually speaks — an offset API, or a page-number
 * API where the head of the first page has to be dropped. Sources disagree on
 * page size and on whether pages exist at all; the caller should not have to care.
 */

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

	/**
	 * @brief Append @p item numbered at its absolute offset and advance next_offset.
	 * Centralizes the `from + size()` bookkeeping every page builder repeats: the
	 * first item lands at @p from, each subsequent one one past the last, and
	 * next_offset is left pointing just beyond the page.
	 * @param from  Absolute item offset of the first item on this page (GetFilters::from).
	 * @param item  The item to append.
	 */
	void append(pageoff from, T item) {
		const pageoff offset = from + static_cast<pageoff>(results.size());
		results.push_back(PageItem<T>{ .item = std::move(item), .offset = offset });
		next_offset = from + static_cast<pageoff>(results.size());
	}
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

/**
 * @brief Item-offset paging arithmetic shared by page-number and offset APIs.
 *
 * Maps a GetFilters item window onto whatever an endpoint speaks:
 *  - offset APIs send @ref from and @ref want directly (skip stays 0);
 *  - page-number APIs send @ref page and drop @ref skip items from the page head.
 * @ref stride is the API's page size — equal to @ref want when the API lets the
 * caller choose it, or a fixed constant when it does not (e.g. a hardcoded perPage).
 */
struct OffsetPaging {
	/// Absolute 0-based item offset to start from (GetFilters::from).
	pageoff from = 0;
	/// Number of items the caller wants back (already clamped).
	pageoff want = 0;
	/// The API's page size in items.
	pageoff stride = 0;

	/// The @p base-indexed (1-based by default) page number containing @ref from.
	[[nodiscard]] pageoff page(pageoff base = 1) const noexcept {
		return stride > 0 ? from / stride + base : base;
	}
	/// Items to skip at the head of @ref page to land exactly on @ref from.
	[[nodiscard]] pageoff skip() const noexcept {
		return stride > 0 ? from % stride : 0;
	}
};

/**
 * @brief Resolve GetFilters::limit into a concrete item count.
 * @return @p fallback when the caller left the limit unset (page_no_limit),
 *         otherwise the requested limit clamped to @p cap. All in item units.
 */
[[nodiscard]] inline pageoff clamp_limit(const GetFilters& filters, pageoff cap,
                                         pageoff fallback) noexcept {
	const pageoff want = filters.limit == page_no_limit
	                         ? fallback
	                         : static_cast<pageoff>(filters.limit);
	return want < cap ? want : cap;
}
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
