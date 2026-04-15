/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once

#ifdef ANIPARSE_USE_LIBFMT
# include <fmt/format>
namespace aniparse {
# ifdef _MSC_VER
#  pragma detect_mismatch("aniparse_format_version", "fmt_format")
# else
	extern const int fmt_format_yes;
	__attribute__((used)) static inline auto fmt_format_check = fmt_format_yes;
# endif
	using fmt::format;
}
#else // ^^^ ANIPARSE_USE_LIBFMT / !ANIPARSE_USE_LIBFMT vvv
# include <format>
namespace aniparse {
# ifdef _MSC_VER
#  pragma detect_mismatch("aniparse_format_version", "fmt_format")
# else
	extern const int fmt_format_no;
	__attribute__((used)) static inline auto fmt_format_check = fmt_format_no;
# endif
	using std::format;
}
#endif // ANIPARSE_USE_LIBFMT