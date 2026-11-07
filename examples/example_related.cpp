// Resolve a RelatedWork destination URL with the receiving catalog's profile.
#include "ExampleClient.hpp"
#include "ExampleManga.hpp"
#include <aniparse/catalog/CatalogRootGetter.hpp>
#include <aniparse/source/ParserStore.hpp>
#include <coro/sync_wait.hpp>
#include <print>

using namespace aniparse;

class DestinationCatalog final : public CatalogRootGetter {
public:
    using CatalogRootGetter::CatalogRootGetter;
protected:
    RequestTask<CatalogEntry> parse_entry_url(RequestContext, ParsedUrl url) const override {
        if (url.path() != "/manga/first")
            co_return make_request_error(RequestErrorCode::NotFound, "Unknown example work");
        co_return std::make_unique<examples::ExampleManga>();
    }
    RequestTask<CatalogEntry> restore_entry(SerializedGetterData data, MediaKind kind) const override {
        if (kind != MediaKind::Manga || data.url != "first" || !data.params.empty())
            co_return make_request_error(RequestErrorCode::InvalidArguments, "Unknown example identity");
        co_return std::make_unique<examples::ExampleManga>();
    }
};

class DestinationParser final : public Parser {
public:
    ParserInfo info() const override { return {.name = "Destination"}; }
    std::string identifier() const override { return "Destination"; }
    void emplace_domains(EmplaceDomainsSink& sink) const override {
        sink.add_domain("example.com");
        sink.add_domain("legacy.example.com");
    }
    std::optional<std::string> suggest_catalog(const ParsedUrl& url) const override {
        if (url.path() == "/manga/first") return "books";
        return std::nullopt;
    }
protected:
    std::vector<ProfileInfo> profile_descriptors() const override {
        return {{"reader", "Reader account"}};
    }
    std::vector<CatalogInfo> catalog_descriptors() const override {
        return {{.id = "books", .name = "Books", .kinds = catalog_kinds::manga, .profile_id = "reader"}};
    }
    std::unique_ptr<CatalogRootGetter> make_catalog_getter_impl(CatalogInfo catalog) const override {
        return std::make_unique<DestinationCatalog>(std::move(catalog));
    }
};

int main() {
    // As returned by another source's load_related(). It knows a URL, not the
    // destination parser's private serialized format. No foreign handle is made.
    RelatedWork related{
        .kind = MediaKind::Manga, .relation = "source", .title = "Example first",
        .destination_url = "https://legacy.example.com/manga/first",
    };
    ParserStore store;
    store.add_parser(std::make_shared<DestinationParser>());
    auto route = store.route_url(*related.destination_url);
    if (!route || !route->catalog_id) return 1;
    auto root = route->parser->make_catalog_getter(*route->catalog_id);
    auto profile = route->parser->make_profile(root->info().profile_id);
    auto context = profile->make_context(examples::make_client());
    // Routing is local; the registered legacy host need not resolve on the network.
    auto opened = coro::sync_wait(root->parse_url(context, std::move(route->url)));
    if (!opened || !opened->has(related.kind)) return 1;
    auto saved = coro::sync_wait(opened->manga->serialize());
    if (!saved) return 1;
    auto restored = coro::sync_wait(root->from_serialized(*saved, related.kind));
    if (!restored || !restored->manga) return 1;
    std::println("Opened and restored with parser={}, catalog={}, profile={}",
                 route->parser->identifier(), root->info().id, profile->info().id);
}
