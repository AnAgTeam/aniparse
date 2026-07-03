/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <fmt/format.h>
#include <source_location>

namespace aniparse {
using fmt::format;
using fmt::format_string;
} // namespace aniparse

namespace aniparse {
template <typename... Args>
struct SourcedFormatStringHelper {
	template <typename U>
	consteval SourcedFormatStringHelper(U fmt, std::source_location loc = std::source_location::current())
	    : fmt(fmt)
	    , loc(loc) {
	}

	format_string<Args...> fmt;
	std::source_location loc;
};

template <typename... T>
using SourcedFormatString = SourcedFormatStringHelper<std::type_identity_t<T>...>;
} // namespace aniparse