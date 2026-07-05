
include_directories(
	${ASYNCNET_INCLUDE_DIRS}
)

add_executable(aniparse-example-manga-parser
	"${CMAKE_CURRENT_LIST_DIR}/example_manga_parser.cpp"
)

set_property(TARGET aniparse-example-manga-parser PROPERTY CXX_STANDARD 23)

target_link_libraries(aniparse-example-manga-parser PRIVATE aniparse)

# Parsing showcase: loads a saved HTML fixture (no network) and extracts data
# via CSS selectors + JS/JSON. EXAMPLE_DATA_DIR points at the fixtures so the
# binary finds them from any working directory.
add_executable(aniparse-example-parse
	"${CMAKE_CURRENT_LIST_DIR}/example_parse.cpp"
)

set_property(TARGET aniparse-example-parse PROPERTY CXX_STANDARD 23)

target_compile_definitions(aniparse-example-parse
	PRIVATE EXAMPLE_DATA_DIR="${CMAKE_CURRENT_LIST_DIR}/data"
)

target_link_libraries(aniparse-example-parse PRIVATE aniparse)

# Networking showcase: makes real HTTP requests through the curl-backed client
# via request() / request_html() / request_json(). Needs network access.
add_executable(aniparse-example-net
	"${CMAKE_CURRENT_LIST_DIR}/example_net.cpp"
)

set_property(TARGET aniparse-example-net PROPERTY CXX_STANDARD 23)

target_link_libraries(aniparse-example-net PRIVATE aniparse)