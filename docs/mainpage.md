aniparse {#mainpage}
========

**C++23 library for parsing anime, manga, images and video.**

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
  for a selected catalog scope; returns cards with manga, anime, or both getters.
- **aniparse::MangaGetter** and **aniparse::AnimeGetter** — operate on one work:
  metadata, chapters and pages, or episodes and video sources.
- **aniparse::ImagesGetter** — the separate image-container catalog API.

Getters never talk to the network directly. They run through a
**aniparse::RequestContext**, which carries the parser's config, cookies and
logger, and sits on a backend-neutral **aniparse::HttpClient**. A parser only
ever touches neutral request/response types — so the HTTP backend is swappable
without changing a single parser.

**aniparse::ParserStore** holds the registered parsers and does the routing: give
it a URL and it resolves which parser owns it and what kind of content it is.

## Where to start reading

| If you want to… | Start at |
|---|---|
| Write a new source | aniparse::Parser, then aniparse::CatalogRootGetter |
| Issue requests | aniparse::RequestContext — `request`, `request_html`, `request_json` |
| Handle failures | aniparse::RequestError and aniparse::RequestErrorCode |
| Pull data out of HTML | aniparse::html::HTMLDocument, aniparse::html::DOMElementView |
| Pull data out of JSON | aniparse/json/Json.hpp — field lookups that never throw |
| Offer search filters | aniparse::SearchCapabilities, aniparse::validate_query |
| Route a URL to a parser | aniparse::ParserStore, aniparse::ParsedUrl |

## Errors

Requests do not throw for I/O or HTTP failures. `request_html` / `request_json`
fold every such failure into an `expected` value carrying an
aniparse::RequestError — so a parser stays in the error channel rather than
unwinding. Exceptions are reserved for programming errors such as invalid parser declarations.
Unsupported optional getter operations return `RequestErrorCode::NotImplemented`.

## Examples

Runnable examples live in the
[`examples/`](https://github.com/AnAgTeam/aniparse/tree/nightly/examples)
directory: a parser skeleton to copy, offline HTML parsing against a fixture, and
live networking.

Complete, real parsers live in
[aniparse-parsers](https://github.com/AnAgTeam/aniparse-parsers) — sources built
on official public APIs (AniList, Kitsu) that show the whole `Parser`/getter model
end to end.
