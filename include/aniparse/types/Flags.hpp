/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"

namespace aniparse {
using CompatibilitiesFlags = FlagsBitfield<64, struct CompatibilitiesFlagsTag>;
using FilteringFlags       = FlagsBitfield<64, struct FilteringFlagsTag>;
} // namespace aniparse

namespace aniparse::compatibilities_flags {
inline constexpr auto supports_inplace_get  = CompatibilitiesFlags::make_bit(0);
inline constexpr auto supports_images_store = CompatibilitiesFlags::make_bit(1);
inline constexpr auto supports_anime_store  = CompatibilitiesFlags::make_bit(2);
inline constexpr auto supports_manga_store  = CompatibilitiesFlags::make_bit(3);
inline constexpr auto supports_video_store  = CompatibilitiesFlags::make_bit(4);
inline constexpr auto using_custom_store    = CompatibilitiesFlags::make_bit(5);
inline constexpr auto adult_source          = CompatibilitiesFlags::make_bit(6);

inline constexpr auto supports_images_search = CompatibilitiesFlags::make_bit(7);
inline constexpr auto supports_registration  = CompatibilitiesFlags::make_bit(8);
inline constexpr auto supports_voting        = CompatibilitiesFlags::make_bit(9);
inline constexpr auto supports_commenting    = CompatibilitiesFlags::make_bit(10);
inline constexpr auto supports_online_lists  = CompatibilitiesFlags::make_bit(11);

/**
 * All the Paginator<T>::next items will be unique over time if true.
 * Otherwise, the items can duplicate. For example, when you get 10
 * from next, then 3 new items added at the start and you start getting
 * duplicates of 8-10 from previous parsing and only 7 new
 */
inline constexpr auto supports_pagination_uniqueness = CompatibilitiesFlags::make_bit(12);

inline constexpr auto unsupported_feature = CompatibilitiesFlags::make_bit(13);

inline constexpr CompatibilitiesFlags default_flags;
} // namespace aniparse::compatibilities_flags

namespace aniparse::filtering_flags {
inline constexpr auto sort_popularity_desc   = FilteringFlags::make_bit(0);
inline constexpr auto sort_popularity_asc    = FilteringFlags::make_bit(1);
inline constexpr auto sort_rating_desc       = FilteringFlags::make_bit(2);
inline constexpr auto sort_rating_asc        = FilteringFlags::make_bit(3);
inline constexpr auto sort_views_desc        = FilteringFlags::make_bit(4);
inline constexpr auto sort_views_asc         = FilteringFlags::make_bit(5);
inline constexpr auto sort_title_desc        = FilteringFlags::make_bit(6);
inline constexpr auto sort_title_asc         = FilteringFlags::make_bit(7);
inline constexpr auto sort_release_time_desc = FilteringFlags::make_bit(8);
inline constexpr auto sort_release_time_asc  = FilteringFlags::make_bit(9);
inline constexpr auto sort_update_time_desc  = FilteringFlags::make_bit(10);
inline constexpr auto sort_update_time_asc   = FilteringFlags::make_bit(11);
inline constexpr auto sort_downloads_desc    = FilteringFlags::make_bit(12);
inline constexpr auto sort_downloads_asc     = FilteringFlags::make_bit(13);
inline constexpr auto sort_author_desc       = FilteringFlags::make_bit(14);
inline constexpr auto sort_author_asc        = FilteringFlags::make_bit(15);

inline constexpr auto sort_popularity_desc_hour  = FilteringFlags::make_bit(16);
inline constexpr auto sort_popularity_desc_day   = FilteringFlags::make_bit(17);
inline constexpr auto sort_popularity_desc_week  = FilteringFlags::make_bit(18);
inline constexpr auto sort_popularity_desc_month = FilteringFlags::make_bit(19);
inline constexpr auto sort_popularity_desc_year  = FilteringFlags::make_bit(20);

inline constexpr auto sort_popularity   = sort_popularity_desc | sort_popularity_asc;
inline constexpr auto sort_rating       = sort_rating_desc | sort_rating_asc;
inline constexpr auto sort_views        = sort_views_desc | sort_views_asc;
inline constexpr auto sort_title        = sort_title_desc | sort_title_asc;
inline constexpr auto sort_release_time = sort_release_time_desc | sort_release_time_asc;
inline constexpr auto sort_update_time  = sort_update_time_desc | sort_update_time_asc;
inline constexpr auto sort_downloads    = sort_downloads_desc | sort_downloads_asc;
inline constexpr auto sort_author       = sort_author_desc | sort_author_asc;

inline constexpr FilteringFlags default_flags;
} // namespace aniparse::filtering_flags