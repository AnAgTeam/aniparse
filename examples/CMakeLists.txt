
include_directories(
	${ASYNCNET_INCLUDE_DIRS}
)

add_executable(aniparseExample1
	"${CMAKE_CURRENT_LIST_DIR}/example1.cpp"
)

add_executable(aniparseExample2
	"${CMAKE_CURRENT_LIST_DIR}/example2.cpp"
)

add_executable(aniparseExample3
	"${CMAKE_CURRENT_LIST_DIR}/example3.cpp"
)

set_property(TARGET aniparseExample1 PROPERTY CXX_STANDARD 23)
set_property(TARGET aniparseExample2 PROPERTY CXX_STANDARD 23)
set_property(TARGET aniparseExample3 PROPERTY CXX_STANDARD 23)

target_link_libraries(aniparseExample1 PRIVATE aniparse)
target_link_libraries(aniparseExample2 PRIVATE aniparse)
target_link_libraries(aniparseExample3 PRIVATE aniparse)