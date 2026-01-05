
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/libasyncnet")

set(ASYNCNET_LIBRARIES asyncnet)
set(ASYNCNET_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/asyncnet/include)
set(ASYNCNET_FOUND ON)

message(STATUS "Found asyncnet libraries: ${CURLPP_LIBRARIES}")
message(STATUS "Found asyncnet includes: ${ASYNCNET_INCLUDE_DIRS}")