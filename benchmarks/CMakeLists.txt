add_executable(parser-store-benchmark
	ParserStoreBenchmark.cpp
	BenchmarkParser.cpp
)
target_link_libraries(parser-store-benchmark PRIVATE aniparse)
set_property(TARGET parser-store-benchmark PROPERTY CXX_STANDARD 23)

add_library(json-decode-baseline OBJECT json/Entry.cpp)
add_library(json-decode-diagnostic OBJECT json/Entry.cpp)
add_library(json-decode-lazy OBJECT json/Entry.cpp)
target_compile_definitions(json-decode-baseline PRIVATE JSON_DIAGNOSTICS=0)
target_compile_definitions(json-decode-diagnostic PRIVATE JSON_DIAGNOSTICS=1)
target_compile_definitions(json-decode-lazy PRIVATE JSON_DIAGNOSTICS=2)
foreach(target json-decode-baseline json-decode-diagnostic json-decode-lazy)
	target_link_libraries(${target} PRIVATE aniparse)
	set_property(TARGET ${target} PROPERTY CXX_STANDARD 23)
endforeach()
add_executable(json-decode-benchmark json/Benchmark.cpp
	$<TARGET_OBJECTS:json-decode-baseline> $<TARGET_OBJECTS:json-decode-diagnostic>
	$<TARGET_OBJECTS:json-decode-lazy>)
target_link_libraries(json-decode-benchmark PRIVATE aniparse)
set_property(TARGET json-decode-benchmark PROPERTY CXX_STANDARD 23)
