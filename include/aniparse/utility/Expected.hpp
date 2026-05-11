/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once

/**
 * !!!!
 * This header is forward to either std::expected or something else (if C++ version < C++23)
 * It is needed to be very careful which 'expected' binded to aniparse.
 * For example, if the library compiled with tl::expected and you USE std::expected,
 * then it will compile fine to use with ParserStore (probably because virtual? At least
 * MSVC compiler compiles fine with different 'expected'??), it will be undefined behaviour
 * (like MEMORY_ACCESS_VIOLATION or unsetted coroutine value)
 * !!!
 */

#if !defined(ANIPARSE_USE_TL_EXPECTED) && !defined(__cpp_lib_expected)
# define ANIPARSE_USE_TL_EXPECTED
#endif

#ifdef ANIPARSE_USE_TL_EXPECTED
# include <tl/expected.hpp>
namespace aniparse {
# ifdef _MSC_VER
#  pragma detect_mismatch("aniparse_expected_version", "tl_expected")
# else
    extern const int tl_expected_yes;
    /// If you see linker error here, probably the library was compiled with std::expected
    __attribute__((used)) static inline auto tl_expected_check = tl_expected_yes;
# endif

    using tl::expected;
    using tl::unexpected;
}
#else // ^^^ ANIPARSE_USE_TL_EXPECTED / !ANIPARSE_USE_TL_EXPECTED vvv
# include <expected>
namespace aniparse {
# ifdef _MSC_VER
#  pragma detect_mismatch("aniparse_expected_version", "std_expected")
# else
    extern const int tl_expected_no;
    /// If you see linker error here, probably the library was compiled with tl::expected
    __attribute__((used)) static inline auto tl_expected_check = tl_expected_no;
# endif

    using std::expected;
    using std::unexpected;
}
#endif // ANIPARSE_USE_TL_EXPECTED