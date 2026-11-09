# Content catalogs and mixed-media cards

A provider exposes `Parser::catalogs()`: named catalogues with stable IDs, a
`CatalogKinds` mask and catalog-specific `CatalogCapabilities`. Two catalogs
can both offer manga while using different catalogues or content policies. A
mixed catalog advertises both kinds without requiring both getters in every card.

Select a catalog once. Every operation on the resulting root uses that selection:

```cpp
for (const CatalogInfo& catalog : parser->catalogs()) {
    // The UI reads catalog.name, catalog.kinds and catalog.capabilities here.
    auto profile = parser->make_profile(catalog.profile_id);
    auto session = profile->make_session(services);
    auto root = parser->make_catalog_getter(catalog.id);
    if (!root) continue;
    auto page = co_await root->search(session.snapshot(), query, paging);
    if (!page) continue; // Present the request error as appropriate.
    for (auto& row : page->results) {
        auto preview = row.item.preview_info(); // Never fetches details.
        if (row.item.manga) {
            auto identity = co_await row.item.manga->serialize();
        }
    }
}
```

## Catalog selection and metadata

`CatalogInfo::id` is an opaque provider-local key. IDs must be nonempty and
unique; the factory rejects malformed declarations. An unknown requested ID
returns nullptr without constructing a root. The root owns its descriptor copy,
so clearing the UI's enumeration cannot invalidate its selection.

`kinds` is a strongly typed mask with `catalog_kinds::manga` and
`catalog_kinds::anime`. `capabilities.reading` and `.watching` are landing-page
summaries; other metadata uses `.flags`, such as adult content or suggestions.
These declarations do not gate calls or remove returned cards. In particular an
omitted reading declaration does not disable a working chapter-pages method.
There are no global reading/watching capability bits. The legacy
`Parser::capabilities()` default derives an aggregate store/flag summary from
catalogs instead of requiring a second source declaration.

The mask does not provide type filtering. A provider can expose dedicated manga
and anime catalogs plus a native mixed catalog, each backed by a different class or
one implementation that uses its bound catalog ID. Search filters, latest sorts
and suggestions are obtained from the selected root. An operation absent from a
particular catalog reports NotImplemented; no hidden cross-catalog merging or
network overfetch is performed.

## URL routing

`Parser::suggest_catalog()` returns an owned `std::optional<std::string>` catalog
ID. `ParserStore::route_url()` validates that ID against the declared catalogs
and carries it into `UrlRoute`. Consumers select
`route.parser->make_catalog_getter(*route.catalog_id)` and pass the parsed URL to
that root. Never infer the catalog ID from a media kind: several catalogs can
have the same kind, and a mixed catalog can contain several formats.

Image-container sources override `valid_for_url()` to recognize their post/pool
URLs without selecting a content catalog. Their routes carry no `catalog_id`;
consumers use `make_images_getter()`. There is no legacy type hint.

## Cards and source implementations

Search, latest, URL opening and identity restoration return `CatalogEntry`: an
owned manga getter, an owned anime getter, or both. Item APIs remain specialized:
chapter pages belong to `MangaGetter`, episode sources to `AnimeGetter`.

Every manga/anime catalog implementation derives directly from
`CatalogRootGetter` and inherits its descriptor constructor. Implement the
protected search/latest/support/open/restore hooks that the source provides.
Existing page builders can append concrete unique_ptr values directly; the
card's converting constructor fills the matching field. The parser's
`make_catalog_getter_impl(catalog)` passes the selected descriptor to that constructor.
There are no format-specific root interfaces, aliases or intermediate bases.

To return a card with both source-grouped formats:

```cpp
CatalogEntry entry{std::make_unique<MyMangaGetter>(manga_id)};
entry.anime = std::make_unique<MyAnimeGetter>(anime_id);
entry.preferred_preview = MediaKind::Anime;
page.append(paging.from, std::move(entry));
```

Group only records the source itself groups. Do not join by matching titles or
IDs. Cards own their getters and are move-only. Empty successful cards
fail with UnexpectedResponse. Native offsets, continuation and total counts are
otherwise preserved, including cards with formats omitted from UI metadata.

## Preview and identity

`CatalogEntry::preview_info()` reads getter previews and returns common
presentation fields. A consumer preference overrides the source hint; missing
previews fall back to the other getter. Without a preference the stable order is
manga, then anime. With neither preview, nullopt is returned without requesting
details. Listing and detail covers remain independent.

Serialization still belongs to each item getter. Persist provider ID, catalog ID,
media kind and the unchanged parser-owned blob. Restore with the selected root's
`from_serialized(blob, kind)`. Equal IDs across catalogs and formats are allowed;
the requested format is checked against the restored card. Existing parser blob
formats are unchanged.

## Boundaries and verification

Manga and anime use only the catalog-aware catalog factory. Image-container
catalogues remain on
their legacy API and are not advertised as a newly supported unified kind.
Existing provider registrations and parser identities are preserved; this API
change does not merge previously distinct providers. Configuration, authentication
and mirrors now belong to request profiles selected by each catalog. See
[request profiles](request-profiles.md) for context creation and persistence.
Database storage, Swift bindings, reader interfaces and consumer caches remain
consumer responsibilities.

Contract tests exercise multiple same-kind catalogs, mixed and grouped cards,
selection across all catalog operations, metadata that does not gate actions,
preview fallback, native pagination, URL routing, malformed catalog declarations
and catalog/format-qualified identity. Real parser integration tests use canned
responses and validate registered sources' descriptors and factories offline.
