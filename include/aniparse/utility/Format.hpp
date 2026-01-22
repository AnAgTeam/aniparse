/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#ifndef ANIPARSE_USE_LIBFMT
# include <format>
#else
# include <fmt/format.h>
#endif

namespace aniparse {
#ifndef ANIPARSE_USE_LIBFMT
	using std::format;
#else
	using fmt::format;
#endif
}