
add_executable(aniparseExtract
	"${CMAKE_CURRENT_LIST_DIR}/AniparseExtract.cpp"
)

set_property(TARGET aniparseExtract PROPERTY CXX_STANDARD 23)

target_link_libraries(aniparseExtract PRIVATE aniparse)