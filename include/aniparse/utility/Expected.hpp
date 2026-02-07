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

#if defined(ANIPARSE_USE_TL_EXPECTED) || !defined(__cpp_lib_expected)
# include <tl/expected.hpp>
namespace aniparse {
    template<typename T, typename E>
    using expected = tl::expected<T, E>;

    template<typename E>
    using unexpected = tl::unexpected<E>;
}
#else
# include <expected>
namespace aniparse {
    template<typename T, typename E>
    using expected = std::expected<T, E>;

    template<typename E>
    using unexpected = std::unexpected<E>;
}
#endif
