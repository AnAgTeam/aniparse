// A mixed catalog can return manga-only, anime-only and source-grouped cards.
#include "ExampleClient.hpp"
#include "ExampleManga.hpp"
#include <aniparse/catalog/CatalogRootGetter.hpp>
#include <aniparse/source/Parser.hpp>
#include <coro/sync_wait.hpp>
#include <array>
#include <print>

using namespace aniparse;

class ExampleAnime final : public AnimeGetter {
public:
    AnimeGetterCapabilities capabilities() const noexcept override { return {}; }
    std::optional<AnimeInfo> preview_info() const noexcept override {
        return AnimeInfo{.id = "adaptation", .common = {.title = "Example adaptation"}};
    }
    RequestTask<AnimeInfo> load_info(RequestContext) const override { co_return *preview_info(); }
    RequestTask<PageResults<VideoSource>> load_episode_sources(
        RequestContext, AnimeEpisodeRef, GetFilters, std::optional<AnimeTrackID>) const override {
        co_return make_request_error(RequestErrorCode::NotImplemented, "Metadata only");
    }
    std::string canonical_url(RequestContext) const override { return "https://example.com/anime/adaptation"; }
    RequestTask<SerializedGetterData> serialize() const override {
        co_return SerializedGetterData{.url = "adaptation"};
    }
};

class MixedCatalog final : public CatalogRootGetter {
public:
    using CatalogRootGetter::CatalogRootGetter;
protected:
    RequestTask<PageResults<CatalogEntry>> latest_items(RequestContext, GetFilters paging) const override {
        enum class FixtureCard { Manga, Anime, Grouped };
        constexpr std::array cards{FixtureCard::Manga, FixtureCard::Anime, FixtureCard::Grouped};
        PageResults<CatalogEntry> page;
        page.total_count = cards.size();
        for (auto offset = paging.from; offset >= 0 && offset < static_cast<pageoff>(cards.size())
             && page.results.size() < paging.limit; ++offset) {
            CatalogEntry entry;
            switch (cards[static_cast<size_t>(offset)]) {
            case FixtureCard::Manga: entry.manga = std::make_unique<examples::ExampleManga>(); break;
            case FixtureCard::Anime: entry.anime = std::make_unique<ExampleAnime>(); break;
            case FixtureCard::Grouped:
                // Group only because our fixture source explicitly does so.
                entry.manga = std::make_unique<examples::ExampleManga>();
                entry.anime = std::make_unique<ExampleAnime>();
                entry.preferred_preview = MediaKind::Anime;
                break;
            }
            page.append(paging.from, std::move(entry));
        }
        co_return page;
    }
    RequestTask<CatalogEntry> restore_entry(SerializedGetterData data, MediaKind kind) const override {
        if (data.params.empty()) {
            if (kind == MediaKind::Manga && data.url == "first")
                co_return std::make_unique<examples::ExampleManga>();
            if (kind == MediaKind::Anime && data.url == "adaptation")
                co_return std::make_unique<ExampleAnime>();
        }
        co_return make_request_error(RequestErrorCode::InvalidArguments, "Unknown identity or media kind");
    }
};

class MixedParser final : public Parser {
public:
    ParserInfo info() const override { return {.name = "Mixed example"}; }
    std::string identifier() const override { return "MixedExample"; }
    void emplace_domains(EmplaceDomainsSink&) const override {}
protected:
    std::vector<CatalogInfo> catalog_descriptors() const override {
        return {{.id = "mixed", .name = "All works", .kinds = catalog_kinds::manga | catalog_kinds::anime}};
    }
    std::unique_ptr<CatalogRootGetter> make_catalog_getter_impl(CatalogInfo catalog) const override {
        return std::make_unique<MixedCatalog>(std::move(catalog));
    }
};

int main() {
    MixedParser parser;
    auto catalog = parser.catalogs().front();
    auto profile = parser.make_profile(catalog.profile_id);
    auto session = profile->make_session(examples::make_services());
    auto root = parser.make_catalog_getter(catalog.id);
    auto page = coro::sync_wait(root->latest(session, {}));
    if (!page || page->results.size() != 3) return 1;
    for (auto& row : page->results) {
        auto preview = row.item.preview_info();
        if (!preview) return 1;
        std::println("{}: manga={}, anime={}", preview->title, bool(row.item.manga), bool(row.item.anime));
    }
    auto& grouped = page->results.back().item;
    if (!grouped.manga || !grouped.anime) return 1;
    if (grouped.preview_info(MediaKind::Manga)->title != "Example first") return 1;
    auto saved = coro::sync_wait(grouped.anime->serialize());
    if (!saved) return 1;
    auto restored = coro::sync_wait(root->from_serialized(*saved, MediaKind::Anime));
    if (!restored || !restored->anime) return 1;
}
