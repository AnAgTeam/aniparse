/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <fmt/format.h>
#include <source_location>

namespace aniparse {
// Committed to fmt::format; call it qualified everywhere. No `using fmt::format` here:
// an unqualified `format(...)` inside namespace aniparse lets ADL pull in std::format
// whenever an argument is a std type, and libc++'s std::vformat instantiates the
// floating-point formatter (std::to_chars<float>) — unavailable before iOS 16.3. The
// std/fmt swap seam this alias once served was dropped (fmt is fixed, for ABI stability).
// format_string stays: it is a type alias, not a function, so it does not feed ADL.
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