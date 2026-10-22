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
/**
 * @brief Unit of every offset in this header: a 0-based position of an item in a
 *        listing, counted over the whole listing rather than within one page.
 * Signed, so offset arithmetic (from + size, next - from) cannot silently wrap.
 */
using pageoff = std::ptrdiff_t;
/// GetFilters::limit sentinel: no explicit limit, return as many items as the
/// endpoint is willing to give. @see clamp_limit
inline constexpr size_t page_no_limit = static_cast<size_t>(-1);

/**
 * @brief One item of a page, tagged with where it sits in the whole listing.
 * The offset is what makes a page resumable and mergeable: it identifies the item
 * independently of which request happened to return it, so a consumer can restart
 * a listing mid-way (GetFilters::from) or splice pages together without counting.
 * @tparam T The listed item type (an info struct, or a getter for the item).
 */
template <typename T>
struct PageItem {
	/// The listed item itself.
	T item;
	/// Absolute 0-based offset of @ref item in the listing (not within the page).
	pageoff offset;
};

/**
 * @brief One page of a listing: the items, where to resume, and how large the
 *        listing is.
 * Returned by every listing endpoint (search, latest, items, comments, ...) for
 * the window the caller asked for in GetFilters.
 *
 * There is no separate "has more" flag: an empty @ref results means the window
 * lies past the end of the listing, i.e. the listing is exhausted. To walk a
 * listing, feed @ref next_offset back as GetFilters::from and stop on the first
 * empty page.
 * @tparam T         The listed item type.
 * @tparam Container The storage of the page's items; a vector of PageItem by
 *                   default, so an endpoint that already holds a different
 *                   container can return it as is.
 */
template <typename T, typename Container = std::vector<PageItem<T>>>
struct PageResults {
	/// The items of this page, in listing order, each carrying its absolute offset.
	Container results;
	/// Offset to pass as GetFilters::from to continue after this page — normally
	/// just past the last item returned (@ref append maintains it).
	pageoff next_offset = pageoff(0);
	/// How many items the whole listing holds, when the source reports a total.
	/// Sources that never report one leave parsers with nothing better than the
	/// count seen so far, so treat this as a hint (and a lower bound), not as an
	/// authoritative end-of-listing test — @ref results being empty is that test.
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
	/// The ordering to sort by, e.g. sort_keys::popularity. Open vocabulary: an
	/// endpoint declares the keys it accepts in its SupportedSorts table.
	std::string key;
	/// Most sources default to descending, so it is the default here too
	bool ascending = false;

	/// Two orders are the same iff they name the same key in the same direction.
	friend bool operator==(const SortOrder&, const SortOrder&) = default;
};

/**
 * @brief Declares which directions an endpoint supports for one sort key.
 * Both directions can be offered at once; a key that supports neither is simply
 * not listed in the SupportedSorts table.
 */
struct SortDescriptor {
	/// Whether the endpoint can return this key's order ascending.
	bool ascending  = false;
	/// Whether the endpoint can return this key's order descending — the common
	/// case ("most popular first"), hence the default.
	bool descending = true;
};

/// Sort declaration of one endpoint (search/latest/...), key -> directions
using SupportedSorts = std::map<std::string, SortDescriptor, std::less<>>;

/**
 * @brief How a caller asks a listing endpoint for one window of items: which
 *        items (an offset and a count) and in which order.
 * Every listing endpoint takes it — search, latest, chapters/episodes, comments —
 * so paging and ordering look the same across domains and sources. The window is
 * stated in items, never in pages: OffsetPaging translates it to whatever the
 * endpoint underneath actually speaks.
 *
 * A requested order the endpoint does not offer is not silently dropped: a getter
 * checks the sort against its declared SupportedSorts (validate_sort, or
 * validate_query together with the search filters) and fails the call with
 * RequestErrorCode::InvalidArguments, so a caller never mistakes an unhonoured
 * order for the source's answer. The same holds for the search filters, which
 * travel next to GetFilters in a SearchRequestQuery rather than inside it.
 * @see PageResults, OffsetPaging, SupportedSorts
 */
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

	/**
	 * @brief The page number containing @ref from.
	 * @param base Index of the API's first page: 1 for the usual 1-based page
	 *             parameter, 0 for a 0-based one.
	 * @return The @p base-indexed page holding item @ref from, or @p base itself
	 *         when @ref stride is 0 (no page size known, nothing to divide by).
	 */
	[[nodiscard]] pageoff page(pageoff base = 1) const noexcept {
		return stride > 0 ? from / stride + base : base;
	}
	/**
	 * @brief Items to drop from the head of @ref page to land exactly on @ref from.
	 * Nonzero whenever the requested window does not start on a page boundary,
	 * which is the price of letting the caller page in items while the endpoint
	 * pages in pages.
	 * @return The number of leading items of the page to discard, or 0 when
	 *         @ref stride is 0.
	 */
	[[nodiscard]] pageoff skip() const noexcept {
		return stride > 0 ? from % stride : 0;
	}
};

/**
 * @brief Resolve GetFilters::limit into a concrete item count.
 * Every parser needs the same two source-specific numbers to turn an open-ended
 * limit into a request: what the endpoint gives when asked for nothing, and the
 * most it will give at all.
 * @param filters  The caller's window, read for its limit.
 * @param cap      Largest item count the endpoint accepts (its maximum page size).
 * @param fallback Item count to use when the caller stated no limit.
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
/// Order by popularity over the item's whole life, however the source measures it.
inline constexpr std::string_view popularity   = "popularity";
/// Order by score/rating.
inline constexpr std::string_view rating       = "rating";
/// Order by view count.
inline constexpr std::string_view views        = "views";
/// Order by title, alphabetically in the source's own collation.
inline constexpr std::string_view title        = "title";
/// Order by release time — when the item was actually published.
inline constexpr std::string_view release_time = "rel_time";
/// Order by update time — when the item last changed (a new chapter, an edit).
inline constexpr std::string_view update_time  = "upd_time";
/// Order by download count.
inline constexpr std::string_view downloads    = "downloads";
/// Order by author name.
inline constexpr std::string_view author       = "author";

/// Popularity over a trailing window ("popular today"), usually descending-only
inline constexpr std::string_view popularity_hour  = "popularity_hour";
/// Popularity over the last day. @see popularity_hour
inline constexpr std::string_view popularity_day   = "popularity_day";
/// Popularity over the last week. @see popularity_hour
inline constexpr std::string_view popularity_week  = "popularity_week";
/// Popularity over the last month. @see popularity_hour
inline constexpr std::string_view popularity_month = "popularity_month";
/// Popularity over the last year. @see popularity_hour
inline constexpr std::string_view popularity_year  = "popularity_year";
} // namespace aniparse::sort_keys
