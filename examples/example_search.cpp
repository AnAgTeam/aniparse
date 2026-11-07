// Typed filter metadata, request values, validation and item-offset pagination.
#include "ExampleClient.hpp"
#include "ExampleManga.hpp"
#include <aniparse/catalog/CatalogRootGetter.hpp>
#include <aniparse/source/ParserProfile.hpp>
#include <coro/sync_wait.hpp>
#include <print>

using namespace aniparse;

class FilteredCatalog final : public CatalogRootGetter {
public:
    using CatalogRootGetter::CatalogRootGetter;
protected:
    RequestTask<SearchCapabilities> load_search_capabilities_for(RequestContext) const override {
        co_return SearchCapabilities{.supported_filters = {
            {"genre", {.name = "Genres", .capabilities = SelectionCapabilities{
                .options = {{"drama", {.name = "Drama"}}, {"comedy", {.name = "Comedy"}}},
                .supports_exclusion = true,
            }}},
        }};
    }
    RequestTask<PageResults<CatalogEntry>> search_items(
        RequestContext context, SearchRequestQuery query, GetFilters paging) const override {
        auto capabilities = co_await load_search_capabilities(context);
        if (!capabilities) co_return unexpected(std::move(capabilities.error()));
        auto errors = validate_query(*capabilities, query, paging);
        if (!errors.empty())
            co_return make_request_error(RequestErrorCode::InvalidArguments, describe_search_query_errors(errors));
        // One fixture: "first", tagged drama. A real parser translates these
        // values into its source's query syntax, preserving opaque option tokens.
        bool matches = std::string_view("first").find(query.query) != std::string_view::npos;
        if (auto it = query.filters.find("genre"); it != query.filters.end()) {
            const auto& choice = std::get<SelectionValue>(it->second);
            for (const auto& token : choice.included) matches &= token == "drama";
            for (const auto& token : choice.excluded) matches &= token != "drama";
        }
        PageResults<CatalogEntry> page;
        page.total_count = matches ? 1 : 0;
        if (matches && paging.from == 0 && paging.limit != 0)
            page.append(paging.from, std::make_unique<examples::ExampleManga>());
        co_return page;
    }
};

int main() {
    ParserProfile profile("Example", {"main", "Anonymous"});
    auto context = profile.make_context(examples::make_client());
    FilteredCatalog root({.id = "main", .name = "Manga", .kinds = catalog_kinds::manga});
    SearchRequestQuery query{.query = "first", .filters = {
        {"genre", SelectionValue{.included = {"drama"}, .excluded = {"comedy"}}},
    }};
    auto caps = coro::sync_wait(root.load_search_capabilities(context));
    if (!caps || !validate_query(*caps, query, {}).empty()) return 1;
    auto page = coro::sync_wait(root.search(context, query, {.limit = 1}));
    if (!page || page->results.size() != 1) return 1;
    std::println("Found {} item; resume at {}", page->results.size(), page->next_offset);
    auto end = coro::sync_wait(root.search(context, query, {.from = page->next_offset}));
    if (!end || !end->results.empty()) return 1;
    query.filters["genre"] = SelectionValue{.included = {"unknown"}};
    auto invalid = coro::sync_wait(root.search(context, query, {}));
    if (invalid || invalid.error().code != RequestErrorCode::InvalidArguments) return 1;
    std::println("Closed vocabulary rejects unknown tokens: {}", invalid.error().message);
}
