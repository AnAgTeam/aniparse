# Architecture and naming {#architecture}

The public headers follow subject areas. A domain keeps its value models and
getter interfaces together; model headers never import getters or execution
services. Include the specific header for the type you use.

| Area | Responsibility | Entry points |
| --- | --- | --- |
| `manga/` | Manga metadata, chapters, translations and access | `MangaInfo.hpp`, `MangaGetter.hpp` |
| `anime/` | Anime metadata, episodes, tracks and access | `AnimeInfo.hpp`, `AnimeGetter.hpp` |
| `images/` | Image containers and browsing | `ImageContainerInfo.hpp`, `ImagesGetter.hpp` |
| `media/` | Shared value models and resource transformations | `MediaInfo.hpp`, `Image.hpp`, `RelatedWork.hpp`, `Recommendation.hpp`, `ResourceAdapter.hpp` |
| `catalog/` | Content catalogs, cards, search and pagination | `CatalogInfo.hpp`, `CatalogRootGetter.hpp`, `CatalogEntry.hpp`, `SearchQuery.hpp`, `SearchCapabilities.hpp` |
| `source/` | Parser registration, profiles and authentication | `Parser.hpp`, `ParserInfo.hpp`, `ParserProfile.hpp`, `ProfileSession.hpp`, `SessionConfig.hpp` |
| `source/overrides/` | Signed remote replacements for source settings | `SourceOverrides.hpp`, `ProfileOverrides.hpp`, `SourceOverridesManager.hpp` |
| `net/` | HTTP value types, cookies and transport backends | `Request.hpp`, `Response.hpp`, `HttpClientConfig.hpp`, `HttpClient.hpp` |
| `runtime/` | Applying profile configuration and sharing execution services | `RequestContext.hpp`, `RuntimeServices.hpp`, `Logger.hpp` |
| `html/`, `json/` | Document parsing, model conversion and data access | `HTMLParser.hpp`, `Conversion.hpp`, `Json.hpp` |
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

## Work identity and durable getter data

`MangaID` and `AnimeID` are opaque strings. Preserve a source's numeric key as
text (`"12345"`), or use its stable slug (`"some-title"`) when that is its
identity. Choose the most stable available part: prefer an immutable source key
over a title-derived slug; exclude mirror domains, query parameters and other
URL decoration. Do not hash a slug to manufacture a numeric ID. Keep listing and
detail IDs consistent. An empty ID means the response has no usable identity;
never deduplicate unrelated works by an empty ID.

IDs are qualified by parser, catalog and media kind. Consumers must neither
parse them as numbers nor infer publication or presentation order from them.
Use dates, source ordering or explicit sort fields for that purpose. An Info ID
is descriptive identity; it does not replace the getter's restoration data,
which may need additional source-owned values.

Persist `SerializedGetterData` unchanged alongside parser ID, catalog ID and
media kind. Restore through `CatalogRootGetter::from_serialized()`. A parser may
put an identity-format version in `params["format_version"]`, treat an absent
version as its legacy format, and migrate known older representations while
constructing a current getter. Its next `serialize()` emits the current format;
the consumer explicitly replaces the saved blob after successful restoration
and serialization. Ordinary loading must not discard the old blob on failure.
Reject malformed data and unsupported versions instead of guessing.

Keep accepting previously emitted formats. A format version is independent of
the parser release and `MediaInfo::revision`; raising it does not invalidate a
whole library. It enables conversion only when the old data reliably identifies
the same work. The current restoration API has no `RequestContext`, so do local
format conversions there; a migration requiring authenticated network lookup
needs an explicit context-bearing API. A renamed slug with no reliable mapping
still requires recovery, such as asking the user to find the work again.

## Method names

- Accessors such as `info()`, `preview_info()`, `identifier()` and `capabilities()`
  read available data without a request. In particular `Parser::info()` is local.
- `load_info()`, `load_chapters()`, `load_episodes()` and
  `load_search_capabilities()` may asynchronously obtain data. Cached or static
  responses remain valid implementations of these operations.
- Natural domain operations retain their verbs: `search()`, `authenticate()`,
  `request()`, `resolve_video()` and `serialize()`.
- `Parser::make_profile()`, `make_catalog_getter()` and `make_images_getter()`
  return fresh independently owned objects. Protected `make_profile_impl()` and
  `make_catalog_getter_impl()` hooks construct implementations after ID validation.
  `ParserProfile::make_session()` creates independent account state;
  `ProfileSession::snapshot()` captures operation settings and overrides.
  Passing a session where `RequestContext` is expected captures a snapshot
  implicitly at the call site. Registry `find_*` methods locate existing objects.
  Static resource/selector/pattern factories use `make()`.

`ParserProfile::authenticate(session, credentials)` prepares an isolated mutable
candidate, immediately awaits the profile's `authenticate_impl` hook and returns
a new `ProfileSession`. The hook returns only success/error and cannot retain
references to the candidate. Failure or cancellation leaves the input session
unchanged. The caller explicitly adopts the returned session.

Snapshots share immutable settings and one `OverridesState` generation; they
retain a live, thread-safe cookie jar. A URL setting publishes a small new state
while sharing the existing request configuration. Source identity and built-in
origins belong to a private profile binding populated by the core factory.
Parsers use `context.base_url()` and `context.canonical_base_url()` without
fallback arguments. API and frontend overrides are independent.

`SessionConfig` contains user URL/header/query overrides; credential fields are
reserved for authentication. `session.config()` is a read-only view of effective
request settings, including the live jar. Persist `SessionState` rather than
that runtime configuration.

`session.export_state()` produces portable user choices and declared durable
credentials; `profile->restore_session(services, saved)` validates identity and
restores them over current defaults in a fresh jar. No services, callbacks or
remote overrides are persisted. An absent mirror choice follows current
remote/built-in defaults; an explicit HTTP(S) URL survives remote updates.

For browser login, retain `profile->begin_web_login(session)` from browser
presentation through all `check_web_login` calls. Its origins and application
request defaults stay fixed; changing settings on that interaction is rejected.
A completed login returns an ordinary session whose later operations observe
new overrides. Tasks returned by member coroutines do not retain their receiver; keep that object
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
`net/Response.hpp`; `Result<T>` carries the value or a `RequestError` for an operation. Results,
errors and cancellable task aliases are declared in `utility/Result.hpp`.

`RequestTask<T>` names a cancellable task yielding `Result<T>`. An operation
may return cached or local data without performing network I/O.


## Media transport ownership

`ResourceAdapter` describes source-specific request preparation and response
transforms. The consumer owns a playback operation, its streaming HTTP session,
and cancellation of each resource. A shared session permits concurrent tasks and
connection reuse; it does not require a single connection across origins or put
adapter preparation requests into the same platform session.

Each successful open has its own optional `ResourceHandle`. The consumer delivers
response metadata before bytes, invokes callbacks serially for that handle, and
calls `final()` only after successful EOF. Cancellation or an error destroys the
handle without flushing it. Output queues belong to the consumer and must be
bounded: pause body delivery when the downstream consumer is slower, resume after
it drains, or fail the resource when the queue cannot accept more output. The
adapter must separately bound any internal transformation buffers.

Manifest interpretation and routing remain outside `ResourceAdapter` in the
current API. Local ports and routing identifiers are consumer implementation
details. Moving HLS interpretation into reusable adapter-side components requires
a separate child-resource routing interface; this lifecycle contract does not
introduce that interface or change the current manifest ownership.
