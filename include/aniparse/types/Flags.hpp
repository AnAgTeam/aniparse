/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"

namespace aniparse {
using CompatibilitiesFlags = FlagsBitfield<64, struct CompatibilitiesFlagsTag>;
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

/// The source can suggest search tokens for a partial input (autocomplete). A
/// getter advertises it in SearchCompatibilities::compatibilities. @see suggest
inline constexpr auto supports_suggestions   = CompatibilitiesFlags::make_bit(14);

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