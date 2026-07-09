cmake_minimum_required (VERSION 3.18)

# ---- PREPARE

option(ANIPARSE_BUILD_TESTS "build tests, Default=OFF" OFF)
option(ANIPARSE_BUILD_EXAMPLES "build examples, Default=OFF" OFF)
option(ANIPARSE_BUILD_TOOLS "build tools, Default=OFF" OFF)
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

add_library(aniparse STATIC
	"src/AniParse.cpp" "include/aniparse/AniParse.hpp"
	"include/aniparse/FlagsBitfield.hpp"
	"include/aniparse/detail/BitsetLite.hpp"
	"include/aniparse/detail/StrongBitset.hpp"
	# src/Client.cpp (the curl backend) is added conditionally below.
	"include/aniparse/Client.hpp"
	"include/aniparse/net/CancellingTask.hpp"
	"include/aniparse/net/vendor/CancellingTask.hpp"
	"include/aniparse/net/vendor/Concepts.hpp"
	"src/ClientContext.cpp" "include/aniparse/ClientContext.hpp"
	"src/Headers.cpp" "include/aniparse/Headers.hpp"
	"src/ParsedUrl.cpp" "include/aniparse/ParsedUrl.hpp"
	"include/aniparse/Common.hpp"
	"include/aniparse/CookieJar.hpp"
	"include/aniparse/utility/Format.hpp"
	"include/aniparse/utility/Coroutines.hpp"
	"include/aniparse/utility/Expected.hpp"
	"include/aniparse/utility/Regex.hpp"
	"include/aniparse/utility/UrlEncode.hpp"

	"include/aniparse/types/Ids.hpp"
	"include/aniparse/types/Flags.hpp"
	"include/aniparse/types/Text.hpp"
	"src/types/Model.cpp" "include/aniparse/types/Model.hpp"
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

	"src/anime/Release.cpp" "include/aniparse/anime/Release.hpp"

	"src/images/Image.cpp" "include/aniparse/images/Image.hpp"

	"src/manga/Manga.cpp" "include/aniparse/manga/Manga.hpp"
)

set_property(TARGET aniparse PROPERTY CXX_STANDARD 20)
set_property(TARGET aniparse PROPERTY CXX_STANDARD_REQUIRED ON)

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

target_link_libraries(aniparse PUBLIC
	$<BUILD_INTERFACE:${LEXBOR_LIBRARIES}
		fmt::fmt
		Boost::json
		Boost::regex>
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

if (ANIPARSE_BUILD_TOOLS)
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tools")
endif()

# ---- CONFIGURE PACKAGE

include(CMakePackageConfigHelpers)

configure_package_config_file(
	"${CMAKE_CURRENT_LIST_DIR}/aniparse-config.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/aniparse-config.cmake"
	INSTALL_DESTINATION
		"{CMAKE_INSTALL_LIBDIR}/cmake/aniparse"
	NO_SET_AND_CHECK_MACRO
	NO_CHECK_REQUIRED_COMPONENTS_MACRO
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