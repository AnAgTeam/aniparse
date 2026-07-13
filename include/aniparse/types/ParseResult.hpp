/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/FlagsBitfield.hpp"

namespace aniparse {

using ParseFlags = FlagsBitfield<64, struct ParseFlagsTag>;

namespace parse_flags {
constexpr auto supports_inplace_get = ParseFlags::make_bit(0);

constexpr ParseFlags default_flags;
} // namespace parse_flags

struct ParseQueryResult {

	ParseFlags flags = parse_flags::default_flags;
};

template <typename T>
struct ParseResult {
	T data;
	ParseQueryResult meta;
};

} // namespace aniparse
