cmake_minimum_required (VERSION 3.18)

# ---- PREPARE

option(ANIPARSE_BUILD_TESTS "build tests, Default=OFF" OFF)
option(ANIPARSE_BUILD_EXAMPLES "build examples, Default=OFF" OFF)

# without this vcpkg won't try to install dependencies
if (CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
	message(STATUS "Found vcpkg")
	include(${CMAKE_TOOLCHAIN_FILE})
endif()

project(aniparse VERSION 0.1 LANGUAGES CXX)

# ---- FIND DEPENDENCIES
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(asyncnet REQUIRED)

# ---- ADD LIBRARY

add_library(aniparse STATIC
	"src/AniParse.cpp" "include/aniparse/AniParse.hpp"
	"src/Release.cpp" "include/aniparse/Release.hpp"
	"include/aniparse/Parser.hpp"
	"include/aniparse/ParserStore.hpp"
	"include/aniparse/FlagsBitfield.hpp"
	"include/aniparse/detail/BitsetLite.hpp"
	"include/aniparse/detail/StrongBitset.hpp"
)

set_property(TARGET aniparse PROPERTY CXX_STANDARD 20)
set_property(TARGET aniparse PROPERTY CXX_STANDARD_REQUIRED ON)

target_include_directories(aniparse PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(aniparse PUBLIC
	$<BUILD_INTERFACE:asyncnet>
)

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