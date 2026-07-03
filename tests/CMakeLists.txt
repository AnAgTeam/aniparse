
add_executable(aniparse-tests
	"catch_amalgamated.cpp" "catch_amalgamated.hpp"
	"CoroTest.hpp"
	"BitsetLiteTest.cpp"
	"StrongBitsetTest.cpp"
	"ParserStoreTest.cpp"
	"HTMLParseTests.cpp"
	"CoroutinesTest.cpp"
	"CookiesAndConfigTest.cpp"
	"RequestorContextTest.cpp"
	"SearchValidationTest.cpp"
	"CssSelectorTest.cpp"
)

set_property(TARGET aniparse-tests PROPERTY CXX_STANDARD 23)

target_link_libraries(aniparse-tests PRIVATE aniparse)

add_test(NAME aniparse-tests COMMAND aniparse-tests)