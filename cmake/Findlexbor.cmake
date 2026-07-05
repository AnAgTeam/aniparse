
set(LEXBOR_OPTIMIZATION_LEVEL "" CACHE STRING "Lexbor optimization flag, DEFAULT=") # CMAKE_BUILD_TYPE
set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "Build shared lexbor, default=OFF" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lexbor")

if (LEXBOR_BUILD_SHARED)
	set(LEXBOR_LIBRARIES lexbor)
else()
	set(LEXBOR_LIBRARIES lexbor_static)
endif()

# lexbor's CSS sources embed U+FFFD (the Unicode replacement char) in narrow
# string literals. Under MSVC with a non-UTF-8 code page (e.g. 1251) that raises
# C4566 and mis-encodes the character to '?'. Compile lexbor as UTF-8 to fix both.
if (MSVC)
	target_compile_options(${LEXBOR_LIBRARIES} PRIVATE /utf-8)
endif()

set(LEXBOR_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lexbor/source")
set(LEXBOR_FOUND ON)

message(STATUS "Found lexbor libraries: ${LEXBOR_LIBRARIES}")
message(STATUS "Found lexbor includes: ${LEXBOR_INCLUDE_DIRS}")