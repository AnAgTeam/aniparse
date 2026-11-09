// Minimal metadata-only manga source. Copy this file and ExampleManga.hpp.
// Optional operations keep the base class's NotImplemented defaults.
#include "ExampleClient.hpp"
#include "ExampleManga.hpp"
#include <aniparse/catalog/CatalogRootGetter.hpp>
#include <aniparse/source/Parser.hpp>
#include <coro/sync_wait.hpp>
#include <print>

using namespace aniparse;

class ExampleCatalog final : public CatalogRootGetter {
public:
    using CatalogRootGetter::CatalogRootGetter;
protected:
    RequestTask<PageResults<CatalogEntry>> latest_items(RequestContext, GetFilters paging) const override {
        PageResults<CatalogEntry> page;
        page.total_count = 1; // The fixture contains exactly one work.
        if (paging.from == 0 && paging.limit != 0)
            page.append(paging.from, std::make_unique<examples::ExampleManga>());
        co_return page;
    }
    RequestTask<CatalogEntry> restore_entry(SerializedGetterData data, MediaKind kind) const override {
        if (kind != MediaKind::Manga || data.url != "first" || !data.params.empty())
            co_return make_request_error(RequestErrorCode::InvalidArguments, "Unknown manga identity");
        co_return std::make_unique<examples::ExampleManga>(data.url);
    }
};

class ExampleParser final : public Parser {
public:
    ParserInfo info() const override { return {.name = "Example"}; }
    std::string identifier() const override { return "Example"; }
    void emplace_domains(EmplaceDomainsSink&) const override {} // No URL entry in this minimal source.
protected:
    std::vector<CatalogInfo> catalog_descriptors() const override {
        return {{.id = "main", .name = "Manga", .kinds = catalog_kinds::manga}};
    }
    std::unique_ptr<CatalogRootGetter> make_catalog_getter_impl(CatalogInfo catalog) const override {
        return std::make_unique<ExampleCatalog>(std::move(catalog));
    }
};

int main() {
    ExampleParser parser;
    auto catalog = parser.catalogs().front();
    auto profile = parser.make_profile(catalog.profile_id);
    auto context = profile->make_session(examples::make_services()).snapshot();
    auto root = parser.make_catalog_getter(catalog.id);
    // root and the returned item stay alive through their member coroutine calls.
    auto page = coro::sync_wait(root->latest(context, {}));
    if (!page || page->results.empty()) return 1;
    auto& manga = page->results.front().item.manga;
    auto info = coro::sync_wait(manga->load_info(context));
    if (!info) return 1;
    std::println("{}: {}", info->id, info->common.title);

    // Persist this blob with parser.identifier(), catalog.id and MediaKind::Manga.
    auto saved = coro::sync_wait(manga->serialize());
    if (!saved) return 1;
    auto restored = coro::sync_wait(root->from_serialized(*saved, MediaKind::Manga));
    if (!restored || !restored->manga) return 1;
    auto end = coro::sync_wait(root->latest(context, {.from = page->next_offset}));
    if (!end || !end->results.empty()) return 1;
    std::println("Restored getter; next page is empty.");
}
