aniparse {#mainpage}
========

**C++20 library for parsing anime, manga, images and video.**

These pages are the API reference, generated from the headers. For building,
installing and the project overview, see the
[README on GitHub](https://github.com/AnAgTeam/aniparse).

@warning The library is in an early stage of development; the API may change
substantially.

## The shape of it

A **aniparse::Parser** is one content source. It declares the domains it handles
and exposes *getters* — the objects that actually fetch and parse:

- **aniparse::MangaRootGetter** — search, latest, autocomplete; hands back a
  `MangaGetter` per title, which in turn yields chapters and pages.
- **aniparse::AnimeRootGetter** — the same shape for anime: episodes, then the
  video sources behind each one.
- **aniparse::ImagesGetter** — image containers (posts, pools, galleries).

Getters never talk to the network directly. They run through a
**aniparse::RequestorContext**, which carries the parser's config, cookies and
logger, and sits on a backend-neutral **aniparse::ClientContext**. A parser only
ever touches neutral request/response types — so the HTTP backend is swappable
without changing a single parser.

**aniparse::ParserStore** holds the registered parsers and does the routing: give
it a URL and it resolves which parser owns it and what kind of content it is.

## Where to start reading

| If you want to… | Start at |
|---|---|
| Write a new source | aniparse::Parser, then aniparse::MangaRootGetter |
| Issue requests | aniparse::RequestorContext — `request`, `request_html`, `request_json` |
| Handle failures | aniparse::RequestError and aniparse::RequestErrorCode |
| Pull data out of HTML | aniparse::html::HTMLDocument, aniparse::html::DOMElementView |
| Pull data out of JSON | aniparse/json/Json.hpp — field lookups that never throw |
| Offer search filters | aniparse::SearchCompatibilities, aniparse::validate_query |
| Route a URL to a parser | aniparse::ParserStore, aniparse::ParsedUrl |

## Errors

Requests do not throw for I/O or HTTP failures. `request_html` / `request_json`
fold every such failure into an `expected` value carrying an
aniparse::RequestError — so a parser stays in the error channel rather than
unwinding. Exceptions are reserved for programming errors (e.g. calling a getter
a parser never implemented, which raises aniparse::NotImplementedError).

## Examples

Runnable examples live in the
[`examples/`](https://github.com/AnAgTeam/aniparse/tree/nightly/examples)
directory: a parser skeleton to copy, offline HTML parsing against a fixture, and
live networking.

Complete, real parsers live in
[aniparse-parsers](https://github.com/AnAgTeam/aniparse-parsers) — sources built
on official public APIs (AniList, Kitsu) that show the whole `Parser`/getter model
end to end.
