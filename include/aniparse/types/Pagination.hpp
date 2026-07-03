/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <cstddef>
#include <vector>

namespace aniparse {
using pageoff = std::ptrdiff_t;
inline constexpr size_t page_no_limit = static_cast<size_t>(-1);

template<typename T>
struct PageItem {
	T item;
	pageoff offset;
};

template<typename T, typename Container = std::vector<PageItem<T>>>
struct PageResults {
	Container results;
	pageoff next_offset = pageoff(0);
	size_t total_count = 0;
};

enum class FilterSort {
	None,
	PopularityDesc,
	PopularityAsc,
	RatingDesc,
	RatingAsc,
	ViewsDesc,
	ViewsAsc,
	TitleDesc,
	TitleAsc,
	ReleaseTimeDesc,
	ReleaseTimeAsc,
	UpdateTimeDesc,
	UpdateTimeAsc,
	DownloadsDesc,
	DownloadsAsc,
	AuthorDesc,
	AuthorAsc,

	PopulariryDescHour,
	PopulariryDescWeek,
	PopulariryDescMonth,
	PopulariryDescYear,
};

struct GetFilters {
	pageoff from    = 0;
	size_t limit    = page_no_limit;
	FilterSort sort = FilterSort::None;
};
} // namespace aniparse
