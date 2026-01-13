
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lexbor")

set(LEXBOR_LIBRARIES lexbor)
set(LEXBOR_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lexbor/include")
set(LEXBOR_FOUND ON)

message(STATUS "Found lexbor libraries: ${LEXBOR_LIBRARIES}")
message(STATUS "Found lexbor includes: ${LEXBOR_INCLUDE_DIRS}")