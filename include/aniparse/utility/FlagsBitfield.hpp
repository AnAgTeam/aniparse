/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/detail/StrongBitset.hpp"
#include "aniparse/detail/BitsetLite.hpp"

namespace aniparse {
template <size_t BitCount, typename Tag>
using FlagsBitfield = detail::StrongBitset<detail::BitsetLite<BitCount>, Tag>;
} // namespace aniparse