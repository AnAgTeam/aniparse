/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "TestRequestContext.hpp"
#include "CoroTest.hpp"
#include "aniparse/catalog/CatalogRootGetter.hpp"
#include "aniparse/source/Parser.hpp"
#include "aniparse/source/ParserStore.hpp"

using namespace aniparse;

namespace {

struct NoNetwork : HttpClient {
    RequestTask<HttpResponse> do_request(ConfiguredGetRequest) override {
        FAIL("Catalog and preview fixtures must not request the network");
        co_return make_request_error(RequestErrorCode::NetworkError, "unexpected request");
    }
    RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override {
        co_return make_request_error(RequestErrorCode::NetworkError, "unexpected request");
    }
    RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override {
        co_return make_request_error(RequestErrorCode::NetworkError, "unexpected request");
    }
    void apply_client_config(HttpClientConfig) override {}
    std::shared_ptr<CookieJar> make_cookie_jar() override { return nullptr; }
};

RequestContext context() {
    return test_context(std::make_shared<NoNetwork>());
}

struct ReadingGetter : MangaGetter {
    bool has_preview = true;
    std::string catalog;
    explicit ReadingGetter(bool preview = true, std::string selected = {}) : has_preview(preview), catalog(std::move(selected)) {}
    MangaGetterCapabilities capabilities() const noexcept override { return {}; }
    std::optional<MangaInfo> preview_info() const noexcept override {
        if (!has_preview) return std::nullopt;
        MangaInfo info{};
        info.id = "1";
        info.common.title = "Reading card";
        info.common.previews.push_back(Image{.url = "https://example.org/card.jpg"});
        return info;
    }
    RequestTask<MangaInfo> load_info(RequestContext) const override {
        MangaInfo info{};
        info.id = "1";
        info.common.title = catalog + "Reading detail";
        info.common.previews.push_back(Image{.url = "https://example.org/detail.jpg"});
        co_return info;
    }
    RequestTask<PageResults<MangaPage>> load_chapter_pages(RequestContext, MangaChapterRef, GetFilters, std::optional<MangaTranslationID>) const override {
        co_return PageResults<MangaPage>{};
    }
    std::string canonical_url(RequestContext) const override { return "https://example.org/manga/1"; }
    RequestTask<SerializedGetterData> serialize() const override { co_return SerializedGetterData{.url = "1"}; }
};

struct WatchingGetter : AnimeGetter {
    bool has_preview = true;
    explicit WatchingGetter(bool preview = true) : has_preview(preview) {}
    AnimeGetterCapabilities capabilities() const noexcept override { return {}; }
    std::optional<AnimeInfo> preview_info() const noexcept override {
        if (!has_preview) return std::nullopt;
        AnimeInfo info{};
        info.id = "1";
        info.common.title = "Watching card";
        return info;
    }
    RequestTask<AnimeInfo> load_info(RequestContext) const override {
        AnimeInfo info{};
        info.id = "1";
        info.common.title = "Watching detail";
        co_return info;
    }
    RequestTask<PageResults<VideoSource>> load_episode_sources(RequestContext, AnimeEpisodeRef, GetFilters, std::optional<AnimeTrackID>) const override {
        co_return PageResults<VideoSource>{};
    }
    std::string canonical_url(RequestContext) const override { return "https://example.org/anime/1"; }
    RequestTask<SerializedGetterData> serialize() const override { co_return SerializedGetterData{.url = "1"}; }
};

CatalogEntry paired_entry() {
    CatalogEntry entry{std::make_unique<ReadingGetter>()};
    entry.anime = std::make_unique<WatchingGetter>();
    return entry;
}

struct MixedRoot : CatalogRootGetter {
    using CatalogRootGetter::CatalogRootGetter;
    mutable unsigned calls = 0;
    bool invalid_result = false;

    RequestTask<PageResults<CatalogEntry>> search_items(RequestContext, SearchRequestQuery, GetFilters filters) const override {
        ++calls;
        std::vector<CatalogEntry> native;
        native.emplace_back(std::make_unique<ReadingGetter>());
        native.emplace_back(std::make_unique<WatchingGetter>());
        native.push_back(paired_entry());
        std::vector<CatalogEntry> selected;
        for (auto& entry : native) {
            const bool keep = info().id == "mixed"
                || (info().id == "watching" ? entry.has(MediaKind::Anime) : entry.has(MediaKind::Manga));
            if (keep) selected.push_back(std::move(entry));
        }
        PageResults<CatalogEntry> page;
        page.total_count = selected.size();
        const auto limit = clamp_limit(filters, static_cast<pageoff>(selected.size()), static_cast<pageoff>(selected.size()));
        for (pageoff i = filters.from; i < static_cast<pageoff>(selected.size()) && i < filters.from + limit; ++i) {
            page.append(filters.from, std::move(selected[static_cast<size_t>(i)]));
        }
        if (invalid_result) page.append(filters.from, CatalogEntry{});
        co_return page;
    }
    RequestTask<PageResults<CatalogEntry>> latest_items(RequestContext ctx, GetFilters filters) const override {
        if (info().id != "mixed") co_return make_request_error(RequestErrorCode::NotImplemented, "This catalog has no recent feed");
        co_return co_await search_items(std::move(ctx), {}, filters);
    }
    RequestTask<SearchCapabilities> load_search_capabilities_for(RequestContext) const override {
        SearchCapabilities support;
        support.supported_filters.emplace(info().id, TextCapabilities{});
        co_return support;
    }
    CatalogLatestCapabilities latest_capabilities_for() const noexcept override {
        CatalogLatestCapabilities support;
        if (info().id == "mixed") support.supported_sorts.emplace(info().id, SortDescriptor{.ascending = true});
        return support;
    }
    RequestTask<std::vector<SearchSuggestion>> suggest_items(RequestContext, std::string, std::optional<std::string>) const override {
        co_return std::vector<SearchSuggestion>{{.value = info().id, .label = info().name}};
    }
    RequestTask<CatalogEntry> parse_entry_url(RequestContext, ParsedUrl) const override {
        if (info().id == "mixed") co_return paired_entry();
        if (info().id == "watching") co_return std::make_unique<WatchingGetter>();
        co_return std::make_unique<ReadingGetter>(true, info().id);
    }
    RequestTask<CatalogEntry> restore_entry(SerializedGetterData data, MediaKind kind) const override {
        if (data.url != "1") co_return make_request_error(RequestErrorCode::InvalidArguments, "invalid identity");
        if (kind == MediaKind::Manga) co_return std::make_unique<ReadingGetter>(false, info().id);
        co_return std::make_unique<WatchingGetter>();
    }
};

struct FixtureProvider : Parser {
    mutable unsigned creations = 0;
    bool omit_anime_metadata = false;
    bool duplicate_ids = false;
    bool empty_id = false;
    bool wrong_binding = false;
    bool invalid_route = false;
    bool unselected_route = false;

    ParserInfo info() const override { return {.name = "Fixture provider"}; }
    std::string identifier() const override { return "fixture"; }
    void emplace_domains(EmplaceDomainsSink& sink) const override { sink.add_domain("example.org"); }
    std::optional<std::string> suggest_catalog(const ParsedUrl& url) const override {
        if (invalid_route) return std::optional<std::string>{"undeclared"};
        if (unselected_route) return std::nullopt;
        const std::string_view path = url.path();
        if (path.starts_with("/reading/")) return std::optional<std::string>{"reading"};
        if (path.starts_with("/other-reading/")) return std::optional<std::string>{"other-reading"};
        if (path.starts_with("/watching/")) return std::optional<std::string>{"watching"};
        if (path.starts_with("/mixed/")) return std::optional<std::string>{"mixed"};
        return std::nullopt;
    }
    std::vector<CatalogInfo> catalog_descriptors() const override {
        std::vector<CatalogInfo> result{
            {.id = "reading", .name = "Reading", .kinds = catalog_kinds::manga, .capabilities = {.reading = true}},
            {.id = "other-reading", .name = "Another manga catalog", .kinds = catalog_kinds::manga},
            {.id = "watching", .name = "Watching", .kinds = catalog_kinds::anime, .capabilities = {.watching = true}},
            {.id = "mixed", .name = "Mixed", .kinds = catalog_kinds::manga | catalog_kinds::anime,
                .capabilities = {.reading = true, .watching = true, .flags = capabilities_flags::adult_source}},
        };
        if (omit_anime_metadata) result.back().kinds = catalog_kinds::manga;
        if (duplicate_ids) result.back().id = result.front().id;
        if (empty_id) result.front().id.clear();
        return result;
    }
    std::unique_ptr<CatalogRootGetter> make_catalog_getter_impl(CatalogInfo catalog) const override {
        ++creations;
        if (wrong_binding) catalog.id = "another-catalog";
        return std::make_unique<MixedRoot>(std::move(catalog));
    }
};

} // namespace

TEST_CASE("Scope IDs select independent catalogues even when their kinds coincide", "[content-catalog]") {
    FixtureProvider provider;
    auto metadata = provider.catalogs();
    REQUIRE(metadata.size() == 4);
    CHECK(metadata[0].kinds == metadata[1].kinds);
    CHECK(metadata[0].capabilities.reading);
    CHECK_FALSE(metadata[1].capabilities.reading);
    auto first = provider.make_catalog_getter(metadata[0].id);
    auto second = provider.make_catalog_getter(metadata[1].id);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->info().id == "reading");
    CHECK(second->info().id == "other-reading");
    CHECK(first->info().capabilities.reading);
    CHECK_FALSE(second->info().capabilities.reading);
    metadata.clear();
    CHECK(first->info().name == "Reading");
    CHECK_FALSE(provider.make_catalog_getter("unknown"));
    CHECK_FALSE(provider.make_catalog_getter(""));
    CHECK(provider.creations == 2);
    auto summary = provider.capabilities().flags;
    CHECK(summary.has(capabilities_flags::supports_manga_store));
    CHECK(summary.has(capabilities_flags::supports_anime_store));
    CHECK(summary.has(capabilities_flags::adult_source));
}

TEST_CASE("Scope factories reject ambiguous identifiers and incorrect binding", "[content-catalog]") {
    FixtureProvider provider;
    provider.duplicate_ids = true;
    CHECK_THROWS_AS(provider.make_catalog_getter("reading"), std::logic_error);
    provider.duplicate_ids = false;
    provider.empty_id = true;
    CHECK_THROWS_AS(provider.make_catalog_getter("mixed"), std::logic_error);
    CHECK(provider.creations == 0);
    provider.empty_id = false;
    provider.wrong_binding = true;
    CHECK_THROWS_AS(provider.make_catalog_getter("mixed"), std::logic_error);
}

TEST_CASE("Catalog card selects previews without requesting details", "[content-catalog]") {
    auto entry = paired_entry();
    REQUIRE(entry.preview_info());
    CHECK(entry.preview_info()->title == "Reading card");
    entry.preferred_preview = MediaKind::Anime;
    CHECK(entry.preview_info()->title == "Watching card");
    CHECK(entry.preview_info(MediaKind::Manga)->title == "Reading card");
    entry.anime = std::make_unique<WatchingGetter>(false);
    CHECK(entry.preview_info()->title == "Reading card");
    entry.manga.reset();
    CHECK_FALSE(entry.preview_info());
    entry.anime.reset();
    CHECK(entry.empty());
    CHECK_FALSE(entry.preview_info());
}

CORO_TEST_CASE("Mixed search keeps card grouping and native continuation", "[content-catalog]") {
    FixtureProvider provider;
    auto root = provider.make_catalog_getter("mixed");
    REQUIRE(root);
    auto first = co_await root->search(context(), {}, {.limit = 2});
    REQUIRE(first);
    REQUIRE(first->results.size() == 2);
    CHECK(first->results[0].item.manga);
    CHECK_FALSE(first->results[0].item.anime);
    CHECK(first->results[1].item.anime);
    CHECK_FALSE(first->results[1].item.manga);
    CHECK(first->total_count == 3);
    CHECK(first->next_offset == 2);
    auto second = co_await root->search(context(), {}, {.from = first->next_offset, .limit = 2});
    REQUIRE(second);
    REQUIRE(second->results.size() == 1);
    CHECK(second->results[0].offset == 2);
    CHECK(second->results[0].item.manga);
    CHECK(second->results[0].item.anime);
    CHECK(second->next_offset == 3);
    auto end = co_await root->search(context(), {}, {.from = second->next_offset, .limit = 2});
    REQUIRE(end);
    CHECK(end->results.empty());
}

CORO_TEST_CASE("All catalog operations use the scope selected at construction", "[content-catalog]") {
    FixtureProvider provider;
    for (const std::string id : {"reading", "watching"}) {
        auto root = provider.make_catalog_getter(id);
        auto page = co_await root->search(context(), {}, {});
        REQUIRE(page);
        REQUIRE(page->results.size() == 2);
        CHECK(page->total_count == 2);
        CHECK(page->results[0].item.has(id == "reading" ? MediaKind::Manga : MediaKind::Anime));
        CHECK(page->results[1].item.manga);
        CHECK(page->results[1].item.anime);
        auto filters = co_await root->load_search_capabilities(context());
        REQUIRE(filters);
        CHECK(filters->supported_filters.contains(id));
        auto suggestions = co_await root->suggest(context(), "");
        REQUIRE(suggestions);
        REQUIRE(suggestions->size() == 1);
        CHECK(suggestions->front().value == id);
        auto denied = co_await root->latest(context(), {});
        REQUIRE_FALSE(denied);
        CHECK(denied.error().code == RequestErrorCode::NotImplemented);
        CHECK(root->latest_capabilities().supported_sorts.empty());
    }
    auto mixed = provider.make_catalog_getter("mixed");
    auto latest = co_await mixed->latest(context(), {});
    REQUIRE(latest);
    CHECK(latest->results.size() == 3);
    CHECK(mixed->latest_capabilities().supported_sorts.contains("mixed"));
}

CORO_TEST_CASE("Scope kind and capability summaries never gate implemented operations", "[content-catalog]") {
    FixtureProvider provider;
    provider.omit_anime_metadata = true;
    auto mixed = provider.make_catalog_getter("mixed");
    CHECK_FALSE(mixed->info().kinds.has(catalog_kinds::anime));
    auto page = co_await mixed->search(context(), {}, {});
    REQUIRE(page);
    CHECK(page->results[1].item.anime);
    auto root = provider.make_catalog_getter("other-reading");
    CHECK_FALSE(root->info().capabilities.reading);
    auto opened = co_await root->parse_url(context(), *ParsedUrl::parse("https://example.org/manga/1"));
    REQUIRE(opened);
    REQUIRE(opened->manga);
    auto pages = co_await opened->manga->load_chapter_pages(context(), MangaChapterRef{}, {}, std::nullopt);
    REQUIRE(pages);
}

CORO_TEST_CASE("Empty cards fail without silently filtering a native page", "[content-catalog]") {
    MixedRoot root{CatalogInfo{.id = "mixed"}};
    root.invalid_result = true;
    auto result = co_await root.search(context(), {}, {});
    REQUIRE_FALSE(result);
    CHECK(result.error().code == RequestErrorCode::UnexpectedResponse);
}

CORO_TEST_CASE("Scope and format disambiguate otherwise identical durable blobs", "[content-catalog]") {
    FixtureProvider provider;
    auto mixed = provider.make_catalog_getter("mixed");
    auto opened = co_await mixed->parse_url(context(), *ParsedUrl::parse("https://example.org/work/1"));
    REQUIRE(opened);
    REQUIRE(opened->manga);
    REQUIRE(opened->anime);
    auto manga_id = co_await opened->manga->serialize();
    auto anime_id = co_await opened->anime->serialize();
    REQUIRE(manga_id);
    REQUIRE(anime_id);
    CHECK(manga_id->url == anime_id->url);
    auto manga = co_await mixed->from_serialized(*manga_id, MediaKind::Manga);
    auto anime = co_await mixed->from_serialized(*anime_id, MediaKind::Anime);
    REQUIRE(manga);
    REQUIRE(anime);
    CHECK(manga->manga);
    CHECK_FALSE(manga->anime);
    CHECK(anime->anime);
    CHECK_FALSE(anime->manga);
    auto other = provider.make_catalog_getter("other-reading");
    auto restored = co_await other->from_serialized(*manga_id, MediaKind::Manga);
    REQUIRE(restored);
    auto details = co_await restored->manga->load_info(context());
    REQUIRE(details);
    CHECK(details->common.title == "other-readingReading detail");
}

CORO_TEST_CASE("Detail metadata does not overwrite a listing preview", "[content-catalog]") {
    CatalogEntry entry{std::make_unique<ReadingGetter>()};
    auto detail = co_await entry.manga->load_info(context());
    REQUIRE(detail);
    CHECK(detail->common.previews.front().url == "https://example.org/detail.jpg");
    CHECK(entry.preview_info()->previews.front().url == "https://example.org/card.jpg");
}

CORO_TEST_CASE("URL routing selects catalog ID independently of media kind", "[content-catalog]") {
    ParserStore store;
    auto provider = std::make_shared<FixtureProvider>();
    store.add_parser(provider);
    auto first = store.route_url("https://example.org/reading/1");
    auto second = store.route_url("https://example.org/other-reading/1");
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first->catalog_id);
    REQUIRE(second->catalog_id);
    CHECK(*first->catalog_id == "reading");
    CHECK(*second->catalog_id == "other-reading");
    auto root = second->parser->make_catalog_getter(*second->catalog_id);
    REQUIRE(root);
    auto entry = co_await root->parse_url(context(), std::move(second->url));
    REQUIRE(entry);
    REQUIRE(entry->manga);
    auto info = co_await entry->manga->load_info(context());
    REQUIRE(info);
    CHECK(info->common.title == "other-readingReading detail");
    auto mixed = store.route_url("https://example.org/mixed/1");
    REQUIRE(mixed);
    REQUIRE(mixed->catalog_id);
    CHECK(*mixed->catalog_id == "mixed");
    CHECK_FALSE(store.route_url("https://example.org/unknown/1"));
    provider->invalid_route = true;
    CHECK_FALSE(store.route_url("https://example.org/reading/1"));
    provider->invalid_route = false;
    provider->unselected_route = true;
    CHECK_FALSE(store.route_url("https://example.org/reading/1"));
}
