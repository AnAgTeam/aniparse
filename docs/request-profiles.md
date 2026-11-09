# Request profiles and authentication

A parser declares catalogs and request profiles independently. Each
`CatalogInfo::profile_id` selects a descriptor from `Parser::profiles()`. Several
catalogs can share one profile. An anonymous source also has a profile for its
headers and origins; the default declaration is `main`.

`ParserProfile` owns immutable behavior. Its `configure`, `mirrors` and
`canonical_base_url` supply built-in defaults. `auth_info` describes the sign-in
flow and `auth_keys` declares durable credentials. Account state belongs to
`ProfileSession`.

## Factory ownership and identity

Implement `make_profile_impl(ProfileInfo)` and construct the derived profile from
that descriptor. The public `Parser::make_profile` validates the returned profile
and binds the owning parser ID automatically. A profile cannot select or replace
its own parser identity. Catalog roots retain their own catalog and profile IDs.

Profile and catalog factories return fresh `unique_ptr` objects. The consumer
owns retention and reuse. A session owns its private profile binding and can
outlive both the profile and parser. Multiple calls to `make_session` create
independent accounts; copies of an existing session share its settings and jar.

## Consumer flow

```cpp
auto catalog = parser->catalogs().front();
auto profile = parser->make_profile(catalog.profile_id);
auto session = profile->make_session(services);
auto root = parser->make_catalog_getter(catalog.id);
auto page = co_await root->latest(session, paging);

auto authenticated = co_await profile->authenticate(session, credentials);
if (authenticated) {
    session = std::move(*authenticated);
    auto saved = session.export_state();
    // Persist saved under its parser_id/profile_id and the consumer's account key.
}
```

`profile->restore_session(services, saved)` validates the saved identity, reapplies
current defaults and restores credentials into a fresh backend jar. Persisted
state contains user choices and declared durable credentials, never services,
callbacks, remote overrides or a live jar. Effective request settings are readable
through `session.config()`.

Declare common request settings in `ParserProfile::configure()`. Constant headers
and parameters go into `ParserConfig` directly; values derived from an origin or
the final request belong in `config.modifiers`. The core calls each modifier with
`HttpRequest&` and `const RequestContext&` after merging defaults. For example:

```cpp
void ExampleProfile::configure(ParserConfig& config) const {
    config.headers.set("Accept", "application/json");
    config.modifiers.emplace_back([](HttpRequest& request, const RequestContext& context) {
        std::visit([&](auto& value) {
            value.headers.set("Referer", std::string(context.canonical_base_url()));
        }, request);
    });
}
```

This rule enforces the profile's frontend origin on every request. Getters do not
repeat it. A modifier reads the supplied snapshot rather than capturing a live
session or a URL at session creation. It must support concurrent calls and must
not retain its arguments or capture a borrowed profile/parser. Passing the snapshot
by reference adds no ownership copies or allocations to snapshot creation.

A session implicitly converts to an owning `RequestContext` when passed to a
getter. Each conversion captures a fresh snapshot at the call site, even if the
returned task is stored and awaited later. Settings, origins, selectors, patterns
and application headers stay fixed across suspension; cookies evolve in the
captured jar. Nested calls reuse the captured context.

To share one environment across several operations, capture it explicitly:

```cpp
auto context = session.snapshot();
auto capabilities = co_await root->load_search_capabilities(context);
auto page = co_await root->search(context, query, paging);
```

The caller selects the matching profile for each catalog/getter. Extractors retain
their own `make_context()` factories and extractor-facing services.

## Authentication implementation

Override `authenticate_impl(AuthenticationSession&, AuthenticationData)`. The
core creates a clean candidate with profile defaults, inherited user options
and a fresh jar. Mutate `candidate.config()` and make login requests through
`candidate.context()`. Return success or an error; the core publishes an
independent session only after success. Failure and cancellation leave the input
session untouched.

The wrapper immediately awaits the hook. Candidate references, copied contexts
and background tasks must not escape it. The caller keeps the profile alive until
the returned task completes or is destroyed: member coroutines borrow `this`.

For interactive login, retain `profile->begin_web_login(session)` from browser
presentation through every `check_web_login` call. Resolve `auth_info` and browser
observations from that pinned session. Cookie-based profiles override
`check_web_login_impl`; storage-token profiles can use the default implementation.
A completed check returns a new ordinary session for explicit adoption.

## Application request defaults

```cpp
HttpClientConfig settings;
settings.request_defaults.headers.set("User-Agent", "MyApp/1.0");
client->set_config(std::move(settings));
```

Request headers take precedence over effective profile headers, which take
precedence over application defaults. Names are case-insensitive. Modifiers run
after merging and may replace values to enforce a source's protocol requirements.
Application headers are captured once per operation; an existing
snapshot keeps them while a new snapshot observes the latest publication.
Transport proxy, timeout, retry and TLS settings remain backend-owned.

## Mirrors and overrides

User choices live in `SessionConfig`. `session.set_mirror(url)` selects a full
HTTP(S) fetch base; `nullopt` follows the current override or built-in default.
The independent `canonical_base_url` setting controls the frontend origin.
Getters call `context.base_url()` and `context.canonical_base_url()` without
supplying seeds. `session.mirror_choices()` returns the current candidate URLs.

`OverridesStore` publishes complete immutable generations containing mirrors,
frontend origins, selectors and patterns. Compiled sets are cached within that
generation by owner and type. Snapshot creation shares immutable owners without
copying configuration tables; changing only a URL also shares the request config.

Schema version 1 accepts an optional `profiles` table inside each parser entry:

```json
{
  "schema_version": 1,
  "revision": 12,
  "parsers": {
    "Example": {
      "profiles": {
        "reader": {
          "mirrors": ["https://api.example"],
          "canonical_base_url": "https://reader.example"
        },
        "viewer": {
          "mirrors": ["https://video-api.example"],
          "canonical_base_url": "https://viewer.example"
        }
      }
    }
  }
}
```

`decode_overrides` verifies the signature over the original payload bytes before
reading profile settings. `SourceOverridesManager` publishes one `OverridesState`
generation for parser requests. Existing request contexts keep
their old snapshots; newly constructed contexts see the applied revision.
Malformed profile tables, IDs, mirror lists or origins reject the whole payload
without replacing the active state. Unknown fields remain ignored.

Obsolete `mirrors` and `canonical_base_url` fields directly inside a parser
entry are ignored, even if malformed. They do not affect requests or routing.
An omitted or empty profile mirror list selects built-in mirrors. Extractors
retain their separate owner-level mirrors because they have no parser profiles.

A profile declares its static frontend through `ParserProfile::canonical_base_url`.
The generator writes a `profiles` entry for every declared profile and obtains
mirrors from `mirrors()`. It emits no parser-wide origin fields. Signature
verification and schema version are unchanged.

`SourceOverridesManager` registers hosts from profile mirrors and frontend origins with
the owning parser's domain routing. The parser still owns URL classification
into catalogs through `Parser::suggest_catalog`; profile IDs are not
implicitly treated as catalog IDs.
