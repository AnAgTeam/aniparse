cmake_minimum_required (VERSION 3.18)

# ---- PREPARE

option(ANIPARSE_BUILD_TESTS "build tests, Default=OFF" OFF)
option(ANIPARSE_BUILD_EXAMPLES "build examples, Default=OFF" OFF)
option(ANIPARSE_BUILD_TOOLS "build tools, Default=OFF" OFF)
option(ANIPARSE_USE_TL_EXPECTED "Use tl::expected instead or std::expected if possible, Default=ON" ON)
option(ANIPARSE_USE_LIBFMT "Use libfmt's 'fmt::format instead of std::format, Default=OFF" OFF)
option(ANIPARSE_USE_BOOST_REGEX "Use boost::regex instead of std::regex, Default=OFF" OFF)

# without this vcpkg won't try to install dependencies
if (CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
	message(STATUS "Found vcpkg")
	include(${CMAKE_TOOLCHAIN_FILE})
endif()

project(aniparse VERSION 0.1 LANGUAGES CXX)

# ---- FIND DEPENDENCIES
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(asyncnet REQUIRED)
find_package(lexbor REQUIRED)
if (ANIPARSE_USE_LIBFMT)
	find_package(fmt REQUIRED)
endif()
if (ANIPARSE_USE_BOOST_REGEX)
	find_package(boost REQUIRED COMPONENTS regex)
endif()

# ---- ADD LIBRARY

add_library(aniparse STATIC
	"src/AniParse.cpp" "include/aniparse/AniParse.hpp"
	"include/aniparse/FlagsBitfield.hpp"
	"include/aniparse/detail/BitsetLite.hpp"
	"include/aniparse/detail/StrongBitset.hpp"
	"src/Client.cpp" "include/aniparse/Client.hpp"
	"include/aniparse/Requests.hpp"
	"src/ClientContext.cpp" "include/aniparse/ClientContext.hpp"
	"src/Common.cpp" "include/aniparse/Common.hpp"
	"include/aniparse/utility/Format.hpp"
	"include/aniparse/utility/Coroutines.hpp"
	"include/aniparse/utility/Expected.hpp"
	"include/aniparse/utility/Regex.hpp"
	"src/utility/Utility.cpp"

	"src/html/HTMLDocument.cpp" "include/aniparse/html/HTMLDocument.hpp"
	"src/html/DOMAttributes.cpp" "include/aniparse/html/DOMAttributes.hpp"
	"src/html/DOMCollection.cpp" "include/aniparse/html/DOMCollection.hpp"
	"src/html/DOMElement.cpp" "include/aniparse/html/DOMElement.hpp"
	"src/html/DOMNode.cpp" "include/aniparse/html/DOMNode.hpp"
	"src/html/HTMLParser.cpp" "include/aniparse/html/HTMLParser.hpp"
	"src/html/JSParser.cpp" "include/aniparse/html/JSParser.hpp"

	"src/ParserStore.cpp" "include/aniparse/ParserStore.hpp"
	"src/Parser.cpp" "include/aniparse/Parser.hpp"

	"src/anime/Release.cpp" "include/aniparse/anime/Release.hpp"

	"src/images/Image.cpp" "include/aniparse/images/Image.hpp"

 "src/utility/Utility.cpp"	"src/manga/Manga.cpp" "include/aniparse/manga/Manga.hpp"
)

set_property(TARGET aniparse PROPERTY CXX_STANDARD 20)
set_property(TARGET aniparse PROPERTY CXX_STANDARD_REQUIRED ON)

target_compile_options(aniparse PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

if (ANIPARSE_USE_TL_EXPECTED)
	target_compile_definitions(aniparse PUBLIC -DANIPARSE_USE_TL_EXPECTED=1)
endif()

target_include_directories(aniparse PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(aniparse PUBLIC
	$<BUILD_INTERFACE:asyncnet ${LEXBOR_LIBRARIES}>
)

if (ANIPARSE_USE_LIBFMT)
	target_link_libraries(aniparse PUBLIC
		$<BUILD_INTERFACE:fmt>
	)

	target_compile_definitions(aniparse PUBLIC -DANIPARSE_USE_LIBFMT=1)
endif()
if (ANIPARSE_USE_BOOST_REGEX)
	target_compile_definitions(aniparse PUBLIC -DANIPARSE_USE_BOOST_REGEX=1)
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