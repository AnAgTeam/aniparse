
# Backend-agnostic logic tests: build and run regardless of the HTTP backend
# (libcoro-driven coroutine tests included — libcoro is always available).
add_executable(aniparse-tests
	"catch_amalgamated.cpp" "catch_amalgamated.hpp"
	"CoroTest.hpp"
	"BitsetLiteTest.cpp"
	"TimeTest.cpp"
	"HeadersTest.cpp"
	"UrlEncodeTest.cpp"
	"UrlPathTest.cpp"
	"PaginationTest.cpp"
	"ParsedUrlTest.cpp"
	"ModelTest.cpp"
	"AuthExportTest.cpp"
	"StrongBitsetTest.cpp"
	"ParserStoreTest.cpp"
	"VideoExtractorTest.cpp"
	"ResourceAdapterTest.cpp"
	"CatalogTest.cpp"
	"CatalogManagerTest.cpp"
	"HTMLParseTests.cpp"
	"HtmlTextTest.cpp"
	"CoroutinesTest.cpp"
	"SearchValidationTest.cpp"
	"CssSelectorTest.cpp"
	"SelectorSourceTest.cpp"
	"JsParserTest.cpp"
	"JsonAccessTest.cpp"
	"ResourceCacheTest.cpp"
	"RepublishableTest.cpp"
	"FileCachingClientTest.cpp"
	"ImagesDefaultsTest.cpp"
)

set_property(TARGET aniparse-tests PROPERTY CXX_STANDARD 23)
target_link_libraries(aniparse-tests PRIVATE aniparse)
add_test(NAME aniparse-tests COMMAND aniparse-tests)

# Tests that drive the curl backend (AsyncClient) directly. Only built when that
# backend is present; with it off they cannot compile (AsyncClient is gated out).
if (ANIPARSE_CURL_BACKEND)
	add_executable(aniparse-tests-backend
		"catch_amalgamated.cpp" "catch_amalgamated.hpp"
		"MangaDefaultsTest.cpp"
		"CookiesAndConfigTest.cpp"
		"RequestorContextTest.cpp"
		"MirrorSourceTest.cpp"
	)

	set_property(TARGET aniparse-tests-backend PROPERTY CXX_STANDARD 23)
	target_link_libraries(aniparse-tests-backend PRIVATE aniparse)
	add_test(NAME aniparse-tests-backend COMMAND aniparse-tests-backend)
endif()
