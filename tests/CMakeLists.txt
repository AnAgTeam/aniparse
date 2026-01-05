
add_executable(aniparseTests
	"catch_amalgamated.cpp" "catch_amalgamated.hpp"
	"CoroTest.hpp"
	"BitsetLiteTest.cpp"
	"StrongBitsetTest.cpp"
)

set_property(TARGET aniparseTests PROPERTY CXX_STANDARD 23)

target_link_libraries(aniparseTests PRIVATE 
	aniparse
)