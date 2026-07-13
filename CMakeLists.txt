cmake_minimum_required (VERSION 3.18)

# ---- PREPARE

option(ANIPARSE_BUILD_TESTS "build tests, Default=OFF" OFF)
option(ANIPARSE_BUILD_EXAMPLES "build examples, Default=OFF" OFF)
option(ANIPARSE_ANALYZE "MSVC /analyze lifetime gate (slow; CI leg)" OFF)

# HTTP backends. The parse core is backend-neutral (talks only to ClientContext);
# each backend is an optional concrete implementation. libasyncnet (hence curl) is
# pulled in only for the curl backend. With no backend the core still builds — the
# consumer injects its own ClientContext.
option(ANIPARSE_CURL_BACKEND "build the curl HTTP backend (pulls libasyncnet + curl)" ON)
option(ANIPARSE_NSURLSESSION_BACKEND "build the NSURLSession HTTP backend (Apple)" OFF)

# Select vcpkg manifest features from the backend options, before the toolchain
# loads (this drives which optional deps, e.g. curl, actually get installed).
if (ANIPARSE_CURL_BACKEND)
	list(APPEND VCPKG_MANIFEST_FEATURES "curl-backend")
endif()

# without this vcpkg won't try to install dependencies
if (CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
	message(STATUS "Found vcpkg")
	include(${CMAKE_TOOLCHAIN_FILE})
endif()

project(aniparse VERSION 0.1 LANGUAGES CXX)

# Defines CMAKE_INSTALL_INCLUDEDIR / LIBDIR / BINDIR used by the install() rules
# below; without it a fresh configure fails with "install ... no DESTINATION".
include(GNUInstallDirs)

# ---- SHARED VS STATIC
#
#     cmake --preset x64-debug -D ANIPARSE_SHARED=ON      (or -D BUILD_SHARED_LIBS=ON)
#
# Deliberately NOT driven by BUILD_SHARED_LIBS at build time, only seeded from it.
# That variable is global and the vendored subprojects own it: libcoro does
# `set(BUILD_SHARED_LIBS ... CACHE INTERNAL "")`, which hijacks it into the cache
# (and, being pre-3.21, wipes any normal variable shadowing it). It would then
# build itself as a DLL while forcing CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS off —
# i.e. a DLL exporting nothing, an empty import lib, and every coro:: symbol
# unresolved when linking us. So: consume the value into our own option, then
# clear BUILD_SHARED_LIBS so every dep stays static and gets absorbed into us.
option(ANIPARSE_SHARED "build libaniparse as a shared library, Default=OFF" ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "vendored deps are always static" FORCE)

if (ANIPARSE_SHARED)
	set(aniparse_library_type SHARED)
	# The deps are static libraries absorbed into the .so/.dylib, so they must be
	# PIC. Set before the add_subdirectory() calls in the find modules below.
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	# Windows has no rpath — the loader looks for a DLL next to the .exe. Put the
	# library and every in-tree consumer (tests, examples) in one directory,
	# so libaniparse.dll AND the third-party DLLs vcpkg's applocal step copies
	# beside it (libcurl, zlib — needed by the library, not by the exe, so an exe's
	# own applocal pass never fetches them) are all found without a copy step.
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
else()
	set(aniparse_library_type STATIC)
endif()

# Windows exports come from the WINDOWS_EXPORT_ALL_SYMBOLS target property below:
# CMake scans the target's own objects and generates a .def, so there is no
# __declspec and no annotation in the headers. That is only sound because nothing
# here exports DATA (which the .def route cannot carry) — every constant in the
# public headers is `inline constexpr`, i.e. compiled into the consumer's TU and
# never fetched from the library. Adding a non-inline global to a public header
# would silently break a shared consumer at link time; keep them inline.

# ---- FIND DEPENDENCIES
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# asyncnet (and thus curl) is a dependency of the curl backend only.
if (ANIPARSE_CURL_BACKEND)
	find_package(asyncnet REQUIRED)
endif()
find_package(lexbor REQUIRED)
find_package(fmt REQUIRED)
find_package(Boost REQUIRED COMPONENTS json regex)

# ---- ADD LIBRARY

add_library(aniparse ${aniparse_library_type}
	"src/AniParse.cpp" "include/aniparse/AniParse.hpp"
	"include/aniparse/utility/FlagsBitfield.hpp"
	"include/aniparse/detail/BitsetLite.hpp"
	"include/aniparse/detail/StrongBitset.hpp"
	# src/Client.cpp (the curl backend) is added conditionally below.
	"include/aniparse/Client.hpp"
	"include/aniparse/net/CancellingTask.hpp"
	"include/aniparse/net/vendor/CancellingTask.hpp"
	"include/aniparse/net/vendor/Concepts.hpp"
	"src/ClientContext.cpp" "include/aniparse/ClientContext.hpp"
	"include/aniparse/net/CookieJar.hpp"
	"include/aniparse/utility/Format.hpp"
	"include/aniparse/utility/Coroutines.hpp"
	"include/aniparse/utility/Expected.hpp"
	"include/aniparse/utility/Regex.hpp"
	"include/aniparse/utility/UrlEncode.hpp"

	"include/aniparse/types/Ids.hpp"
	"include/aniparse/types/Flags.hpp"
	"include/aniparse/types/Text.hpp"
	"src/types/Model.cpp" "include/aniparse/types/Model.hpp"
	"src/types/Headers.cpp" "include/aniparse/types/Headers.hpp"
	"src/types/ParsedUrl.cpp" "include/aniparse/types/ParsedUrl.hpp"
	"include/aniparse/types/ParserInfo.hpp"
	"include/aniparse/types/ParseResult.hpp"
	"include/aniparse/types/Request.hpp"
	"include/aniparse/types/Pagination.hpp"
	"include/aniparse/types/Response.hpp"
	"src/types/Search.cpp" "include/aniparse/types/Search.hpp"
	"include/aniparse/types/Serialization.hpp"
	"src/types/Authentication.cpp" "include/aniparse/types/Authentication.hpp"

	"src/html/HTMLDocument.cpp" "include/aniparse/html/HTMLDocument.hpp"
	"src/html/DOMAttributes.cpp" "include/aniparse/html/DOMAttributes.hpp"
	"src/html/DOMCollection.cpp" "include/aniparse/html/DOMCollection.hpp"
	"src/html/DOMElement.cpp" "include/aniparse/html/DOMElement.hpp"
	"src/html/DOMNode.cpp" "include/aniparse/html/DOMNode.hpp"
	"src/html/HTMLParser.cpp" "include/aniparse/html/HTMLParser.hpp"
	"src/html/JSParser.cpp" "include/aniparse/html/JSParser.hpp"
	"src/html/CompiledSelector.cpp" "include/aniparse/html/CompiledSelector.hpp"
	"src/html/SelectorSource.cpp" "include/aniparse/html/SelectorSource.hpp"

	"src/ParserStore.cpp" "include/aniparse/ParserStore.hpp"
	"src/Catalog.cpp" "include/aniparse/Catalog.hpp"
	"src/CatalogManager.cpp" "include/aniparse/CatalogManager.hpp"
	"src/Parser.cpp" "include/aniparse/Parser.hpp"

	"src/anime/Anime.cpp" "include/aniparse/anime/Anime.hpp"
	"include/aniparse/anime/AnimeModel.hpp"

	"src/images/Image.cpp" "include/aniparse/images/Image.hpp"
	"include/aniparse/images/ImageModel.hpp"

	"src/manga/Manga.cpp" "include/aniparse/manga/Manga.hpp"

	# Reusable booru engine toolkit: a dialect interface + generic getters and a
	# generic parser bound to a data-only site descriptor. Neutral machinery (names
	# no source); concrete engines/descriptors live in the parsers layer above.
	"include/aniparse/engines/BooruEngine.hpp"
	"include/aniparse/engines/BooruSite.hpp"
	"src/engines/BooruParser.cpp" "include/aniparse/engines/BooruParser.hpp"
	"src/engines/BooruImagesGetter.cpp" "include/aniparse/engines/BooruImagesGetter.hpp"
	"src/engines/BooruContainerGetter.cpp" "include/aniparse/engines/BooruContainerGetter.hpp"
	"src/engines/BooruPoolGetter.cpp" "include/aniparse/engines/BooruPoolGetter.hpp"
)

set_property(TARGET aniparse PROPERTY CXX_STANDARD 20)
set_property(TARGET aniparse PROPERTY CXX_STANDARD_REQUIRED ON)

set_target_properties(aniparse PROPERTIES
	# Ignored for a static build; drives the .def generation for a shared one.
	WINDOWS_EXPORT_ALL_SYMBOLS ON
	# libaniparse.so.0.1 with an SONAME of libaniparse.so.0 (no-op when static).
	VERSION "${PROJECT_VERSION}"
	SOVERSION "${PROJECT_VERSION_MAJOR}"
)

target_compile_options(aniparse PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_compile_options(aniparse PUBLIC
	$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# Opt-in MSVC static lifetime analysis (a separate, slow CI leg — not per-build).
# The C++ Core Guidelines lifetime rules catch dangling views/pointers at compile
# time (e.g. `std::string_view v = json::str(...)` -> C26815/C26449). Scoped to our
# sources only (PRIVATE), system headers excluded, the lifetime rules promoted to
# errors so a regression fails the build; the noisier style rules stay non-fatal.
if (ANIPARSE_ANALYZE AND MSVC)
	target_compile_options(aniparse PRIVATE
		/analyze /analyze:external- /analyze:plugin EspXEngine.dll
		/external:env:INCLUDE /external:W0
		# Lifetime rules -> errors: a dangling view/pointer/use-after-move fails.
		/we26815 /we26816 /we26449 /we26800 /we26811
		# Silence the gsl-everywhere style profile (noexcept/const/gsl::at/span/
		# narrow_cast/bounds/switch) — opinionated, not correctness; keeps the gate
		# signal-only. 26818 (switch coverage) also fires inside vendored lexbor.
		/wd26440 /wd26446 /wd26447 /wd26455 /wd26439 /wd26432 /wd26818
		/wd26472 /wd26481 /wd26482 /wd26821 /wd26496 /wd26497 /wd26475 /wd26459)
endif()

target_include_directories(aniparse PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# fmt / Boost are find_package'd imported targets a consumer recreates via
# find_dependency (see aniparse-config.cmake.in), so they belong on the install
# interface too — not only BUILD_INTERFACE. lexbor is a vendored static lib the
# install does not ship, so it stays build-only.
target_link_libraries(aniparse PUBLIC
	fmt::fmt
	Boost::json
	Boost::regex
	$<BUILD_INTERFACE:${LEXBOR_LIBRARIES}>
)

# ---- HTTP BACKENDS (optional, concrete ClientContext implementations)

if (ANIPARSE_CURL_BACKEND)
	# The macro is PUBLIC on purpose: the public header net/CancellingTask.hpp
	# selects the real asyncnet task type vs the vendored copy off it, so every
	# consumer TU must see the same value as the library was built with.
	target_compile_definitions(aniparse PUBLIC ANIPARSE_CURL_BACKEND=1)
	target_sources(aniparse PRIVATE "src/Client.cpp")
	target_link_libraries(aniparse PUBLIC $<BUILD_INTERFACE:asyncnet>)
endif()

if (ANIPARSE_NSURLSESSION_BACKEND)
	target_compile_definitions(aniparse PUBLIC ANIPARSE_NSURLSESSION_BACKEND=1)
	# target_sources(aniparse PRIVATE "src/net/NSURLSessionClient.mm")
	# target_link_libraries(aniparse PUBLIC "-framework Foundation")
endif()

# ---- COROUTINE RUNTIME (libcoro)

# libcoro is the coroutine runtime behind NetworkTask/CancellingTask and the
# coroutine helpers (utility/Coroutines.hpp) — needed regardless of the HTTP
# backend. The curl backend already pulls it in transitively via asyncnet, so
# add it directly only when that backend is off; adding it twice would redefine
# the target.
if (NOT ANIPARSE_CURL_BACKEND)
	set(LIBCORO_BUILD_EXAMPLES OFF)
	set(LIBCORO_BUILD_TESTS OFF)
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/libasyncnet/libcoro")
	target_link_libraries(aniparse PUBLIC $<BUILD_INTERFACE:libcoro>)
endif()

if (ANIPARSE_BUILD_TESTS)
	enable_testing()
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
endif()

if (ANIPARSE_BUILD_EXAMPLES)
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/examples")
endif()

# ---- CONFIGURE PACKAGE

include(CMakePackageConfigHelpers)

configure_package_config_file(
	"${CMAKE_CURRENT_LIST_DIR}/aniparse-config.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/aniparse-config.cmake"
	INSTALL_DESTINATION
		"${CMAKE_INSTALL_LIBDIR}/cmake/aniparse"
	NO_SET_AND_CHECK_MACRO
	NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/aniparse-config-version.cmake"
	VERSION "${PROJECT_VERSION}"
	COMPATIBILITY SameMajorVersion
)

configure_file("aniparse.pc.in" "aniparse.pc" @ONLY)

# always prefix with lib*
set_target_properties(aniparse PROPERTIES PREFIX "lib")

install(DIRECTORY "include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aniparse.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(TARGETS aniparse
	EXPORT aniparse-targets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT aniparse-targets
	NAMESPACE aniparse::
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/aniparse"
	EXPORT_LINK_INTERFACE_LIBRARIES
)

# The package config + version file themselves — without this find_package(aniparse)
# finds no config and imports nothing.
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/aniparse-config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/aniparse-config-version.cmake"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/aniparse"
)