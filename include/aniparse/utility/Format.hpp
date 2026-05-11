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

	using fmt::format_string;
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

	using std::format_string;
	using std::format;
}
#endif // ANIPARSE_USE_LIBFMT

namespace aniparse {
	template<typename ... Args>
	struct SourcedFormatStringHelper {
		template<typename U>
		consteval SourcedFormatStringHelper(U fmt, std::source_location loc = std::source_location::current())
			: fmt(fmt), loc(loc) {

		}

		format_string<Args ...> fmt;
		std::source_location loc;
	};

	template<typename ... T>
	using SourcedFormatString = SourcedFormatStringHelper<std::type_identity_t<T> ...>;
}