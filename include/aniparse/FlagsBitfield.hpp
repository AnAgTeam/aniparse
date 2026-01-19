/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#pragma once
#include "aniparse/detail/StrongBitset.hpp"
#ifdef __cpp_lib_constexpr_bitset
# include <bitset>
#else
# include "aniparse/detail/BitsetLite.hpp"
#endif

namespace aniparse {
	template<size_t BitCount, typename Tag>
#ifdef __cpp_lib_constexpr_bitset
	using FlagsBitfield = detail::StrongBitset<std::bitset<BitCount>, Tag>;
#else
	using FlagsBitfield = detail::StrongBitset<detail::BitsetLite<BitCount>, Tag>;
#endif
}