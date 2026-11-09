
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
	"PolymorphicInterfacesTest.cpp"
	"AuthExportTest.cpp"
	"ParserProfileTest.cpp"
	"RequestorContextTest.cpp"
	"MirrorSourceTest.cpp"
	"StrongBitsetTest.cpp"
	"ParserStoreTest.cpp"
	"VideoExtractorTest.cpp"
	"ResourceAdapterTest.cpp"
	"CatalogTest.cpp"
	"CatalogManagerTest.cpp"
	"ProfileCatalogTest.cpp"
	"HTMLParseTests.cpp"
	"HtmlTextTest.cpp"
	"NumberTest.cpp"
	"CoroutinesTest.cpp"
	"SearchValidationTest.cpp"
	"CssSelectorTest.cpp"
	"SelectorSourceTest.cpp"
	"RegexSourceTest.cpp"
	"CatalogSinkTest.cpp"
	"JsParserTest.cpp"
	"JsonAccessTest.cpp"
	"JsonViewTest.cpp"
	"ResourceCacheTest.cpp"
	"RepublishableTest.cpp"
	"FileCachingClientTest.cpp"
	"ImagesDefaultsTest.cpp"
	"ContentCatalogTest.cpp"
	"MangaDefaultsTest.cpp"
)

set_property(TARGET aniparse-tests PROPERTY CXX_STANDARD 23)
target_link_libraries(aniparse-tests PRIVATE aniparse)
add_test(NAME aniparse-tests COMMAND aniparse-tests)

# Tests that drive the curl backend (CurlHttpClient) directly. Only built when that
# backend is present; with it off they cannot compile (CurlHttpClient is gated out).
if (ANIPARSE_CURL_BACKEND)
	add_executable(aniparse-tests-backend
		"catch_amalgamated.cpp" "catch_amalgamated.hpp"
		"CookiesAndConfigTest.cpp"
	)

	set_property(TARGET aniparse-tests-backend PROPERTY CXX_STANDARD 23)
	target_link_libraries(aniparse-tests-backend PRIVATE aniparse)
	add_test(NAME aniparse-tests-backend COMMAND aniparse-tests-backend)
endif()

# Each value header is compiled in isolation with only the model target's include
# directory. Runtime/transport libraries and their third-party include paths are absent.
set(aniparse_model_headers
    media/Image.hpp media/ImageResource.hpp media/Tags.hpp media/Identity.hpp media/RelatedWork.hpp
    media/Publication.hpp media/Rating.hpp media/Community.hpp media/MediaInfo.hpp
    media/Text.hpp media/Ids.hpp media/Serialization.hpp
    manga/MangaInfo.hpp anime/AnimeInfo.hpp images/ImageContainerInfo.hpp
    source/ParserInfo.hpp source/ProfileInfo.hpp
    catalog/SearchQuery.hpp catalog/SearchCapabilities.hpp catalog/CatalogInfo.hpp catalog/Pagination.hpp
    AniParse.hpp source/overrides/ProfileOverrides.hpp
)
set(aniparse_model_checks)
foreach(model_header IN LISTS aniparse_model_headers)
    string(REPLACE "/" "_" check_name "${model_header}")
    set(check_source "${CMAKE_CURRENT_BINARY_DIR}/headers/${check_name}.cpp")
    file(GENERATE OUTPUT "${check_source}" CONTENT "#include <aniparse/${model_header}>\n")
    list(APPEND aniparse_model_checks "${check_source}")
endforeach()
add_library(aniparse-model-headers OBJECT ${aniparse_model_checks})
target_link_libraries(aniparse-model-headers PRIVATE aniparse::models)
