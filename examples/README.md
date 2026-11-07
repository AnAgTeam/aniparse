# Runnable examples

Start with the minimal manga source, then pick the independent feature you need.
The executable targets use C++23 for `std::println`; the core/model API is C++20.

| Source | Target | What to copy or learn |
|---|---|---|
| [Minimal manga parser](example_manga_parser.cpp) + [getter](ExampleManga.hpp) | `aniparse-example-manga-parser` | Parser declaration, catalog factory, default profile, metadata, paging and durable restoration |
| [Authentication](example_auth.cpp) | `aniparse-example-auth` | Token descriptor, fresh login config, adoption, export/restore and failed-login isolation |
| [Mixed catalog](example_mixed_catalog.cpp) | `aniparse-example-mixed_catalog` | Single-format and grouped cards, previews, independent media identities |
| [Search filters](example_search.cpp) | `aniparse-example-search` | Capability metadata, selected values, inclusion/exclusion and validation |
| [Related work URL](example_related.cpp) | `aniparse-example-related` | Legacy-domain routing, destination profile and getter persistence |
| [Recommendations](example_similar.cpp) | `aniparse-example-similar` | Owned getters, source explanations, paging and destination identity |
| [HTML parsing](example_parse.cpp) | `aniparse-example-parse` | DOM, selectors and script data from a saved fixture |
| [Networking](example_net.cpp) | `aniparse-example-net` | Real HTTP, HTML and JSON requests; requires curl and network access |

## Build and run offline

With the dependencies described in the root README installed:

```sh
cmake -S . -B build -DANIPARSE_BUILD_EXAMPLES=ON -DANIPARSE_CURL_BACKEND=OFF
cmake --build build
./build/examples/aniparse-example-manga-parser
./build/examples/aniparse-example-auth
./build/examples/aniparse-example-mixed_catalog
./build/examples/aniparse-example-search
./build/examples/aniparse-example-related
./build/examples/aniparse-example-similar
./build/examples/aniparse-example-parse
```

Run these commands from the library root. Multi-configuration generators may put
executables under an additional configuration directory (for example `Debug`).
Enable `ANIPARSE_CURL_BACKEND` to build the networking example. `ExampleClient.hpp`
provides a backend-free client for offline fixtures when curl is disabled.

## From fixture to source

Copy the minimal parser and `ExampleManga.hpp`; replace the fixture metadata and
opaque `first` slug with actual source data. The fixture has no reader and does
not claim reading capability. The fixture also supplies two recommendations for
`first`; other optional operations inherit `NotImplemented`.
Add chapters/pages, URL opening, search or authentication only when supported.
The other examples use the same fixture getter to keep each feature visible.

Keep getter/profile objects alive through member coroutine completion. Persist
parser ID, catalog ID, media kind and serialized data together. Restore only
known formats, preserving compatibility with data the source previously emitted.
A mixed catalog does not imply both media getters in every card; never group
works merely because their titles or IDs match.

The auth example accepts a synthetic token locally to demonstrate state flow.
A real source verifies credentials with its service on an independent login
context and stores exported credentials through the consumer's secure storage.

Recommendations use `Recommendation<MangaGetter>` or `Recommendation<AnimeGetter>`.
Each page item contains an owned `getter` and a free-text `reason`, empty when the
source supplies no explanation. Retain the getter through its asynchronous calls.
The reason belongs to this recommendation; it is not part of the destination's
metadata or serialized identity. `RelatedWork` separately describes relations,
including cross-media and cross-source destinations without a local getter.
