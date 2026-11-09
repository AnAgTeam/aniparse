aniparse {#mainpage}
========

**C++20 library for parsing anime, manga, images and video.**

These pages are the API reference, generated from the headers. For building,
installing and the project overview, see the
[README on GitHub](https://github.com/AnAgTeam/aniparse).

@warning The library is in an early stage of development; the API may change
substantially.

See @subpage architecture for the header layout, dependency boundaries and naming rules.

## The shape of it

A **aniparse::Parser** is one content source. It declares the domains it handles
and exposes *getters* — the objects that actually fetch and parse:

- **aniparse::CatalogRootGetter** — search, latest, autocomplete and URL opening
  for a selected catalog; returns cards with manga, anime, or both getters.
- **aniparse::MangaGetter** and **aniparse::AnimeGetter** — operate on one work:
  metadata, chapters and pages, or episodes and video sources.
- **aniparse::ImagesGetter** — the separate image-container catalog API.

Getters never talk to the network directly. They run through a
**aniparse::RequestContext**, which carries the parser's config, cookies and
logger, and sits on a backend-neutral **aniparse::HttpClient**. A parser only
ever touches neutral request/response types — so the HTTP backend is swappable
without changing a single parser.

**aniparse::ParserStore** holds the registered parsers and does the routing: give
it a URL and it resolves the owning parser and selected catalog. Media kind is
independent of catalog selection; image-container routes have no catalog ID.

## Where to start reading

| If you want to… | Start at |
|---|---|
| Write a new source | aniparse::Parser, then aniparse::CatalogRootGetter |
| Select catalogs and interpret cards | [Content catalogs](mixed-catalog.md) |
| Create contexts and authenticate | [Request profiles](request-profiles.md) |
| Build and validate search filters | [Search](search.md) |
| Preserve work identity and restore getters | [Architecture](architecture.md), aniparse::SerializedGetterData |
| Read recommendations and their source explanations | aniparse::Recommendation, aniparse::MangaGetter::load_similar, aniparse::AnimeGetter::load_similar |
| Follow related works across sources | aniparse::RelatedWork, aniparse::ParserStore::route_url |
| Apply signed source settings | aniparse::SourceOverridesManager |
| Embed artwork | aniparse::ImageResource, README build helper |
| Issue requests | aniparse::RequestContext — `request`, `request_html`, `request_json` |
| Handle failures | aniparse::RequestError and aniparse::RequestErrorCode |
| Pull data out of HTML | aniparse::html::HTMLDocument, aniparse::html::DOMElementView |
| Decode JSON models | @ref json-models — Boost conversion and explicit tolerant fields |
| Read individual JSON fields | aniparse/json/Json.hpp — field lookups that never throw |
| Offer search filters | aniparse::SearchCapabilities, aniparse::validate_query |
| Route a URL to a parser | aniparse::ParserStore, aniparse::ParsedUrl |

## Ownership and execution

Parser factories return fresh `std::unique_ptr` objects. Select a catalog, create
its declared profile, and use that profile to construct a `RequestContext`.
Catalogs may share a profile without sharing their content identity. Authentication
returns a new configuration; the caller explicitly adopts it.

Member coroutine tasks borrow their receiver. Keep the getter, parser or profile
alive until its task completes or is destroyed. `RequestTask<T>` yields
`Result<T>`; it may complete with cached or local data without network access.
Local accessors such as `info()` on a parser and `preview_info()` on a getter
do not fetch data; getter `load_info()` and other `load_*` methods may.

Persist parser ID, catalog ID, media kind and the unchanged serialized getter
blob. Work IDs are opaque strings, not numbers or ordering keys. Related works
may provide a same-parser handle, external IDs or a destination URL. Route that
URL and create the destination catalog's own profile context before opening it.

The core and model headers require C++20. `expected` uses `tl::expected`, and
constexpr flags use the library's `BitsetLite`. The example and test targets use
C++23; examples use `std::print` / `std::println`. HTTP backends are optional:
use curl, Apple's NSURLSession, or supply an `HttpClient`.

## Errors

Requests do not throw for I/O or HTTP failures. `request_html` / `request_json`
fold every such failure into an `expected` value carrying an
aniparse::RequestError — so a parser stays in the error channel rather than
unwinding. Exceptions are reserved for programming errors such as invalid parser declarations.
Unsupported optional getter operations return `RequestErrorCode::NotImplemented`.

## Examples

Runnable examples live in the
[`examples/`](https://github.com/AnAgTeam/aniparse/tree/nightly/examples)
directory: a minimal manga parser to copy, independent examples of authentication,
mixed catalogs, typed filters, recommendations and related URL routing, offline
HTML parsing, and live networking. See the examples README for build targets and run commands.

Complete, real parsers live in
[aniparse-parsers](https://github.com/AnAgTeam/aniparse-parsers) — sources built
on official public APIs (AniList, Kitsu) that show the whole `Parser`/getter model
end to end.
