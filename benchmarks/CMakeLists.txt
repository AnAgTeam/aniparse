add_executable(parser-store-benchmark
	ParserStoreBenchmark.cpp
	BenchmarkParser.cpp
)
target_link_libraries(parser-store-benchmark PRIVATE aniparse)
set_property(TARGET parser-store-benchmark PROPERTY CXX_STANDARD 23)
