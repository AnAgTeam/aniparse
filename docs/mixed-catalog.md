# Unified catalog experiment

A provider exposes `Parser::scopes()`: named catalogues with stable IDs, a
`CatalogKinds` mask and catalog-specific `CatalogCapabilities`. Two scopes
can both offer manga while using different catalogues or content policies. A
mixed scope advertises both kinds without requiring both getters in every card.

Select a scope once. Every operation on the resulting root uses that selection:

```cpp
for (const CatalogScope& scope : parser->scopes()) {
    // The UI reads scope.name, scope.kinds and scope.capabilities here.
    auto profile = parser->profile(scope.profile_id);
    auto context = base_context.new_with_config(profile->make_config());
    auto root = parser->catalog_getter(scope.id);
    if (!root) continue;
    auto page = co_await root->search(context, query, paging);
    if (!page) continue; // Present the request error as appropriate.
    for (auto& row : page->results) {
        auto preview = row.item.preview_info(); // Never fetches details.
        if (row.item.manga) {
            auto identity = co_await row.item.manga->serialize();
        }
    }
}
```

## Scope selection and metadata

`CatalogScope::id` is an opaque provider-local key. IDs must be nonempty and
unique; the factory rejects malformed declarations. An unknown requested ID
returns nullptr without constructing a root. The root owns its descriptor copy,
so clearing the UI's enumeration cannot invalidate its selection.

`kinds` is a strongly typed mask with `catalog_kinds::manga` and
`catalog_kinds::anime`. `capabilities.reading` and `.watching` are landing-page
summaries; other metadata uses `.flags`, such as adult content or suggestions.
These declarations do not gate calls or remove returned cards. In particular an
omitted reading declaration does not disable a working chapter-pages method.
There are no global reading/watching capability bits. The legacy
`Parser::compatibilities()` default derives an aggregate store/flag summary from
scopes instead of requiring a second source declaration.

The mask does not provide type filtering. A provider can expose dedicated manga
and anime scopes plus a native mixed scope, each backed by a different class or
one implementation that uses its bound scope ID. Search filters, latest sorts
and suggestions are obtained from the selected root. An operation absent from a
particular scope reports NotImplemented; no hidden cross-catalog merging or
network overfetch is performed.

## URL routing

`Parser::suggest_getter()` returns `GetterSuggestionResult`, containing an owned
`scope_id` and an optional legacy `type` hint. `ParserStore::route_url()` carries
that scope ID into `UrlRoute`. Consumers select
`route.parser->catalog_getter(*route.scope_id)` and pass the parsed URL to that
root. Never infer the catalog ID from the media kind: several scopes can have
the same kind. A mixed URL can have a known scope with an Unknown type hint.
Undeclared scope IDs are rejected during routing; a native manga/anime route
without a scope ID is not guessed from the type hint. Legacy image/video routing
remains available without catalog IDs.

## Cards and source implementations

Search, latest, URL opening and identity restoration return `CatalogEntry`: an
owned manga getter, an owned anime getter, or both. Item APIs remain specialized:
chapter pages belong to `MangaGetter`, episode sources to `AnimeGetter`.

Every manga/anime catalog implementation derives directly from
`CatalogRootGetter` and inherits its descriptor constructor. Implement the
protected search/latest/support/open/restore hooks that the source provides.
Existing page builders can append concrete unique_ptr values directly; the
card's converting constructor fills the matching field. The parser's
`make_catalog_getter(scope)` passes the selected descriptor to that constructor.
There are no format-specific root interfaces, aliases or intermediate bases.

To return a card with both source-grouped formats:

```cpp
CatalogEntry entry{std::make_unique<MyMangaGetter>(manga_id)};
entry.anime = std::make_unique<MyAnimeGetter>(anime_id);
entry.preferred_preview = MediaKind::Anime;
page.append(paging.from, std::move(entry));
```

Group only records the source itself groups. Do not join by matching titles or
numeric IDs. Cards own their getters and are move-only. Empty successful cards
fail with UnexpectedResponse. Native offsets, continuation and total counts are
otherwise preserved, including cards with formats omitted from UI metadata.

## Preview and identity

`CatalogEntry::preview_info()` reads getter previews and returns common
presentation fields. A consumer preference overrides the source hint; missing
previews fall back to the other getter. Without a preference the stable order is
manga, then anime. With neither preview, nullopt is returned without requesting
details. Listing and detail covers remain independent.

Serialization still belongs to each item getter. Persist provider ID, scope ID,
media kind and the unchanged parser-owned blob. Restore with the selected root's
`from_serialized(blob, kind)`. Equal IDs across scopes and formats are allowed;
the requested format is checked against the restored card. Existing parser blob
formats are unchanged.

## Boundaries and verification

Manga and anime use only the scope-aware catalog factory. Image-container
catalogues remain on
their legacy API and are not advertised as a newly supported unified kind.
Existing provider registrations and parser identities are preserved; this API
change does not merge previously distinct providers. Configuration, authentication
and mirrors now belong to request profiles selected by each scope. See
[request profiles](request-profiles.md) for context creation and persistence.
The database, Swift bridge, reader interfaces and cache concurrency are outside
this experiment.

Contract tests exercise multiple same-kind scopes, mixed and grouped cards,
selection across all catalog operations, metadata that does not gate actions,
preview fallback, native pagination, URL routing, malformed scope declarations
and scope/format-qualified identity. Real parser integration tests use canned
responses and validate registered sources' descriptors and factories offline.
