/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"

#include <aniparse/catalog/CatalogRootGetter.hpp>
#include <aniparse/manga/MangaGetter.hpp>

#include <memory>

using namespace aniparse;

namespace {

// The default MangaGetter/CatalogRootGetter methods ignore the context, so this
// client only needs to exist; its request paths are never taken.
struct NullClient : HttpClient {
	RequestTask<HttpResponse> do_request(ConfiguredGetRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	void apply_client_config(HttpClientConfig) override {
	}
	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return nullptr;
	}
};

RequestContext make_context() {
	return RequestContext(std::make_shared<NullClient>(), nullptr, std::make_shared<ParserConfig>());
}

// Implements only the pure-virtual surface; every optional override keeps its
// base default (NotImplemented / delegating).
struct StubMangaGetter : MangaGetter {
	MangaGetterCapabilities capabilities() const noexcept override {
		return {};
	}
	RequestTask<MangaInfo> load_info(RequestContext) const override {
		MangaInfo out;
		out.common.title = "stub";
		co_return out;
	}
	RequestTask<PageResults<MangaPage>> load_chapter_pages(
	    RequestContext, MangaChapterRef, GetFilters, std::optional<MangaTranslationID>) const override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	std::string canonical_url(RequestContext) const override {
		return "https://example.com/manga/stub";
	}
	RequestTask<SerializedGetterData> serialize() const override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
};

struct StubCatalogGetter : CatalogRootGetter {};

} // namespace

TEST_CASE("CatalogRootGetter default support declarations are empty") {
	StubCatalogGetter getter;

	auto search = coro::sync_wait(getter.load_search_capabilities(make_context()));
	REQUIRE(search);
	CHECK(search->supported_filters.empty());
	CHECK(search->supported_sorts.empty());

	CatalogLatestCapabilities latest = getter.latest_capabilities();
	CHECK(latest.supported_sorts.empty());
}

CORO_TEST_CASE("CatalogRootGetter search/latest/parse_url default to NotImplemented") {
	StubCatalogGetter getter;
	RequestContext context = make_context();

	auto searched = co_await getter.search(context, SearchRequestQuery{}, GetFilters{});
	REQUIRE_FALSE(searched.has_value());
	CHECK(searched.error().code == RequestErrorCode::NotImplemented);

	auto latest = co_await getter.latest(context, GetFilters{});
	REQUIRE_FALSE(latest.has_value());
	CHECK(latest.error().code == RequestErrorCode::NotImplemented);

	auto parsed = co_await getter.parse_url(context, ParsedUrl::parse("https://example.com/manga/1").value());
	REQUIRE_FALSE(parsed.has_value());
	CHECK(parsed.error().code == RequestErrorCode::NotImplemented);

	auto restored = co_await getter.from_serialized(SerializedGetterData{}, MediaKind::Manga);
	REQUIRE_FALSE(restored.has_value());
	CHECK(restored.error().code == RequestErrorCode::NotImplemented);
}

CORO_TEST_CASE("MangaGetter optional overrides default to NotImplemented") {
	StubMangaGetter getter;
	RequestContext context = make_context();

	auto translations = co_await getter.load_translations(context, GetFilters{});
	REQUIRE_FALSE(translations.has_value());
	CHECK(translations.error().code == RequestErrorCode::NotImplemented);

	auto chapters = co_await getter.load_chapters(context, GetFilters{});
	REQUIRE_FALSE(chapters.has_value());
	CHECK(chapters.error().code == RequestErrorCode::NotImplemented);

	auto related = co_await getter.load_related(context, GetFilters{});
	REQUIRE_FALSE(related.has_value());
	CHECK(related.error().code == RequestErrorCode::NotImplemented);

	auto similar = co_await getter.load_similar(context, GetFilters{});
	REQUIRE_FALSE(similar.has_value());
	CHECK(similar.error().code == RequestErrorCode::NotImplemented);
}

TEST_CASE("MangaGetter preview_info knows nothing until a listing hands it a card") {
	StubMangaGetter getter;

	// Free, synchronous, and empty by default — no fallback to info(), so a caller
	// drawing a list of restored getters cannot accidentally pay a request per row.
	CHECK_FALSE(getter.preview_info().has_value());
}
