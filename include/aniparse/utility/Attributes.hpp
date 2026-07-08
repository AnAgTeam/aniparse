/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once

/**
 * @file
 * Portable spellings of vendor attributes the library relies on. Feature-detected
 * with __has_cpp_attribute so the code never hard-depends on one compiler: a
 * toolchain that understands the attribute gets the diagnostic, any other expands
 * it to nothing and compiles identically.
 */

/**
 * @def ANIPARSE_LIFETIMEBOUND
 * @brief Mark a function parameter (or an implicit object parameter) whose
 * lifetime the return value borrows into. A compiler that understands it warns
 * when the returned reference/pointer/view would outlive a temporary argument
 * (e.g. object_field(parse(body).as_object(), ...)). Not yet standard, so this
 * selects the vendor spelling and no-ops where unavailable.
 */
#if defined(__has_cpp_attribute)
#  if __has_cpp_attribute(msvc::lifetimebound)
#    define ANIPARSE_LIFETIMEBOUND [[msvc::lifetimebound]]
#  elif __has_cpp_attribute(clang::lifetimebound)
#    define ANIPARSE_LIFETIMEBOUND [[clang::lifetimebound]]
#  elif __has_cpp_attribute(gnu::lifetimebound)
#    define ANIPARSE_LIFETIMEBOUND [[gnu::lifetimebound]]
#  endif
#endif
#ifndef ANIPARSE_LIFETIMEBOUND
#  define ANIPARSE_LIFETIMEBOUND
#endif
