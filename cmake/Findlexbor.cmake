
set(LEXBOR_OPTIMIZATION_LEVEL "" CACHE STRING "Lexbor optimization flag, DEFAULT=") # CMAKE_BUILD_TYPE
set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "Build shared lexbor, default=OFF" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lexbor")

if (LEXBOR_BUILD_SHARED)
	set(LEXBOR_LIBRARIES lexbor)
else()
	set(LEXBOR_LIBRARIES lexbor_static)
endif()
set(LEXBOR_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lexbor/source")
set(LEXBOR_FOUND ON)

message(STATUS "Found lexbor libraries: ${LEXBOR_LIBRARIES}")
message(STATUS "Found lexbor includes: ${LEXBOR_INCLUDE_DIRS}")