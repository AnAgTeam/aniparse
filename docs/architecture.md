# Architecture and naming {#architecture}

The public headers follow subject areas. A domain keeps its value models and
getter interfaces together; model headers never import getters or execution
services. Include the specific header for the type you use.

| Area | Responsibility | Entry points |
| --- | --- | --- |
| `manga/` | Manga metadata, chapters, translations and access | `MangaInfo.hpp`, `MangaGetter.hpp` |
| `anime/` | Anime metadata, episodes, tracks and access | `AnimeInfo.hpp`, `AnimeGetter.hpp` |
| `images/` | Image containers and browsing | `ImageContainerInfo.hpp`, `ImagesGetter.hpp` |
| `media/` | Shared value models and resource transformations | `MediaInfo.hpp`, `Image.hpp`, `RelatedWork.hpp`, `ResourceAdapter.hpp` |
| `catalog/` | Content catalogs, cards, search and pagination | `CatalogInfo.hpp`, `CatalogRootGetter.hpp`, `CatalogEntry.hpp`, `SearchQuery.hpp`, `SearchCapabilities.hpp` |
| `source/` | Parser registration, profiles and authentication | `Parser.hpp`, `ParserInfo.hpp`, `ParserProfile.hpp`, `ParserConfig.hpp` |
| `source/overrides/` | Signed remote replacements for source settings | `SourceOverrides.hpp`, `ProfileOverrides.hpp`, `SourceOverridesManager.hpp` |
| `net/` | HTTP value types, cookies and transport backends | `Request.hpp`, `Response.hpp`, `HttpClientConfig.hpp`, `HttpClient.hpp` |
| `runtime/` | Applying profile configuration and sharing execution services | `RequestContext.hpp`, `RuntimeServices.hpp`, `Logger.hpp` |
| `html/`, `json/` | Document parsing and data access | `HTMLParser.hpp`, `Json.hpp` |
| `engines/` | Reusable engines shared by concrete parsers | `booru/` |
| `concepts/`, `utility/` | Shared language constraints and focused helpers | Headers named for their purpose |

## Data and behavior

`ParserInfo` describes the parser's display name, language and artwork.
`CatalogInfo` identifies one selectable content catalog and its capabilities and
request profile. `ProfileInfo` describes a request profile. Their identities are
independent; several catalogs may use one profile.

`Info` denotes descriptive data, not every value type: `Image`, `Comment`, `Tag`
and `Rating` retain their domain names. `Config` denotes settings. `Getter`
interfaces provide access to a catalog or a particular work; their models remain
usable without instantiating them.

`aniparse::models` is a header-only CMake target with no link dependencies on the
runtime or parser stack. Tests compile each model header in isolation using this
target. HTTP headers and URLs may occur in resource descriptions as values,
without importing a transport implementation.

## Method names

- Accessors such as `info()`, `preview_info()`, `identifier()` and `capabilities()`
  read available data without a request. In particular `Parser::info()` is local.
- `load_info()`, `load_chapters()`, `load_episodes()` and
  `load_search_capabilities()` may asynchronously obtain data. Cached or static
  responses remain valid implementations of these operations.
- Natural domain operations retain their verbs: `search()`, `authenticate()`,
  `request()`, `resolve_video()` and `serialize()`.
- `make_*` factories create fresh owned objects. Registry `find_*` methods locate
  an existing object; absence is an ordinary lookup result.

`ParserProfile::authenticate()` returns a fresh authenticated config without
mutating the supplied context. The caller explicitly adopts the result. Tasks
returned by member coroutines do not retain their receiver; keep that object
alive until the task completes or is destroyed. Factory ownership remains with
the consumer through `unique_ptr`.

## Remote overrides and compatibility

`Catalog` refers to browsable content. `SourceOverrides` is the independently
signed remote configuration snapshot. `ProfileOverrides` keeps mirror and
canonical-origin replacements together while preserving their independent
presence. `SourceOverridesManager` verifies and publishes these settings.

The internal reorganization does not change signed payload keys, schema version,
revision checks, parser/profile/catalog IDs, getter serialization, or the
precedence of request headers and defaults. Existing external tooling may retain
legacy command or asset names containing `catalog`; those remain wire/command
compatibility names, not names of C++ configuration types.

Transport receives prepared HTTP requests and session cookies, never a complete
`ParserConfig`. `RequestContext` applies profile settings and application
fallbacks before dispatch. `HttpResponse` describes an HTTP response in
`net/Response.hpp`; operation results, errors and cancellable task aliases are
in `utility/Result.hpp`.
