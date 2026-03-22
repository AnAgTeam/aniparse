
include_directories(
	${ASYNCNET_INCLUDE_DIRS}
)

add_executable(aniparse-example-manga-parser
	"${CMAKE_CURRENT_LIST_DIR}/example_manga_parser.cpp"
)

set_property(TARGET aniparse-example-manga-parser PROPERTY CXX_STANDARD 23)

target_link_libraries(aniparse-example-manga-parser PRIVATE aniparse)