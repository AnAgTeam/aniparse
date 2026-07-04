/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"

#include <aniparse/Client.hpp>
#include <aniparse/manga/Manga.hpp>

#include <memory>

using namespace aniparse;

namespace {

// The default MangaGetter/MangaRootGetter methods ignore the context, so this
// client only needs to exist; its request paths are never taken.
struct NullClient : ClientContext {
	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	void set_config(ClientConfig) override {
	}
	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return nullptr;
	}
};

RequestorContext make_context() {
	return RequestorContext(std::make_shared<NullClient>(), nullptr, std::make_shared<ParserConfig>());
}

// Implements only the pure-virtual surface; every optional override keeps its
// base default (NotImplemented / delegating).
struct StubMangaGetter : MangaGetter {
	MangaGetterCompatibilities compatibilies() const noexcept override {
		return {};
	}
	NetworkRequestTask<MangaInfo> info(RequestorContext) override {
		MangaInfo out;
		out.title = "stub";
		co_return out;
	}
	NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
	    RequestorContext, MangaChapterRef, GetFilters, std::optional<MangaTranslationID>) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<SerializedGetterData> serialize() override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
};

struct StubMangaRootGetter : MangaRootGetter {
	NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(SerializedGetterData) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
};

} // namespace

TEST_CASE("MangaRootGetter default support declarations are empty") {
	StubMangaRootGetter getter;

	SearchCompatibilities search = getter.search_support();
	CHECK(search.supported_filters.empty());
	CHECK(search.supported_sorts.empty());

	MangaGetterRootCompatibilities latest = getter.latest_support();
	CHECK(latest.supported_sorts.empty());
}

TEST_CASE("default_config_from returns null for a null base config") {
	StubMangaRootGetter getter;
	CHECK(getter.default_config_from(nullptr) == nullptr);
}

CORO_TEST_CASE("MangaRootGetter search/latest/parse_url default to NotImplemented") {
	StubMangaRootGetter getter;
	RequestorContext context = make_context();

	auto searched = co_await getter.search(context, SearchRequestQuery{}, GetFilters{});
	REQUIRE_FALSE(searched.has_value());
	CHECK(searched.error().code == RequestErrorCode::NotImplemented);

	auto latest = co_await getter.latest(context, GetFilters{});
	REQUIRE_FALSE(latest.has_value());
	CHECK(latest.error().code == RequestErrorCode::NotImplemented);

	auto parsed = co_await getter.parse_url(context, "https://example.com/manga/1");
	REQUIRE_FALSE(parsed.has_value());
	CHECK(parsed.error().code == RequestErrorCode::NotImplemented);

	auto restored = co_await getter.from_serialized(SerializedGetterData{});
	REQUIRE_FALSE(restored.has_value());
	CHECK(restored.error().code == RequestErrorCode::NotImplemented);
}

CORO_TEST_CASE("MangaGetter optional overrides default to NotImplemented") {
	StubMangaGetter getter;
	RequestorContext context = make_context();

	auto translations = co_await getter.translation_info(context, GetFilters{});
	REQUIRE_FALSE(translations.has_value());
	CHECK(translations.error().code == RequestErrorCode::NotImplemented);

	auto chapters = co_await getter.chapters_info(context, GetFilters{});
	REQUIRE_FALSE(chapters.has_value());
	CHECK(chapters.error().code == RequestErrorCode::NotImplemented);

	auto related = co_await getter.related(context, GetFilters{});
	REQUIRE_FALSE(related.has_value());
	CHECK(related.error().code == RequestErrorCode::NotImplemented);

	auto similar = co_await getter.similar(context, GetFilters{});
	REQUIRE_FALSE(similar.has_value());
	CHECK(similar.error().code == RequestErrorCode::NotImplemented);
}

CORO_TEST_CASE("MangaGetter preview_info delegates to info, and reset is a safe no-op") {
	StubMangaGetter getter;
	RequestorContext context = make_context();

	auto preview = co_await getter.preview_info(context);
	REQUIRE(preview.has_value());
	CHECK(preview->title == "stub");

	getter.reset(); // default no-op must be callable without effect
}
