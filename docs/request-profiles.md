# Request profiles and authentication

A parser declares selectable catalogs and request profiles independently. Each
`CatalogInfo::profile_id` selects a descriptor from `Parser::profiles()`. Several
catalogs can reference one profile, or each can use a different profile. An
anonymous source still has a profile for its headers and mirrors; the default
implementation declares `main` with no authentication or source-specific defaults.

`ParserProfile` owns immutable behavior, not an account or a cookie jar:

- `configure` supplies source-specific headers, query parameters and request hooks.
- `auth_info` describes the existing password, token or interactive browser flow.
- `authenticate` implements authentication on a fresh configuration.
- `auth_keys` and `export_auth` select durable credentials for persistence.
- `mirrors` declares this profile's built-in fetch origins.

The parser implements `make_profile_impl(ProfileInfo)` to construct the appropriate
profile class. It need not override `profile_descriptors()` when the default `main` descriptor
is sufficient. Public `profiles()` validates a nonempty list and nonempty unique
IDs. Public `catalogs()` validates `catalog_descriptors()` IDs and their profile
references, without constructing profiles; an empty catalog list is valid.
Invalid declarations throw `std::logic_error` during enumeration.

`make_profile(id)` and `make_catalog_getter(id)` return nullptr only for unknown IDs.
For a declared ID, a null or mismatched factory result throws `std::logic_error`.
Profiles must match their parser and profile IDs; roots must match their selected
catalog and profile IDs. These exceptions identify implementation defects, rather
than network errors. Both factories continue returning independent owned objects.

## Factory ownership

`Parser::make_profile(id)` returns a fresh `std::unique_ptr<ParserProfile>`, matching
the ownership model of catalog-root factories. The caller chooses whether to
retain it for one operation, a screen, or the entire application session. Parsers
do not cache profile instances or return hidden singletons. A consumer needing
shared ownership can convert its unique pointer to a shared pointer at its own
boundary. Concurrent use must still respect each object's documented contract.

## Consumer flow

```cpp
auto catalog = parser->catalogs().front();
auto profile = parser->make_profile(catalog.profile_id);
auto context = profile->make_context(services);

// Or restore saved authentication into a fresh backend jar:
// auto context = profile->make_context(services, saved_auth);

auto root = parser->make_catalog_getter(catalog.id);
auto page = co_await root->latest(context, paging);
```

`make_context()` is the parser-facing construction path. Each call applies fresh
profile defaults and provisions a backend cookie jar. Parser and profile IDs
live beside the settings in private context storage, with no extra ownership
handle. `ParserConfig` contains settings only; copying or replacing it cannot
change the context's binding. `new_with_config()` copies settings, retains an
existing jar, and rejects null. It preserves the context's override snapshot.

Contexts remain operation arguments, so existing getters can use a newly created
context after transport settings change. Matching a context to the correct
getter remains the caller's responsibility; the common type does not encode a
particular parser identity. Extractors create contexts through `make_context()`
on their extractor or extractor store, using extractor-facing services.

For authentication, pass a context created from the selected profile:

```cpp
auto authenticated = co_await profile->authenticate(context, credentials);
if (authenticated) {
    auto saved = profile->export_auth(**authenticated);
    // Persist saved under (parser->identifier(), profile->info().id).
    context = context.new_with_config(*authenticated);
}
```

Implementations override the virtual `authenticate` method; credentials
and context are passed by value. The context must belong to the selected profile.
The task borrows its profile: the caller must keep the profile alive until the
task completes or is destroyed. In particular, storing or returning a task does
not transfer ownership of its profile. A direct `co_await` on a temporary owning
profile handle keeps that handle alive until the await expression finishes.
A failed login leaves the caller's configuration and cookie jar untouched.
Successful authentication returns a new configuration for the application to
adopt for subsequent requests.

Two catalogs share an account only when the application supplies their shared
profile's authenticated context. Merely retrieving the same profile ID does not
implicitly share cookies, and one profile can create multiple independent
account configurations. Nothing mutable is stored in the parser or profile.

## Application request defaults

```cpp
HttpClientConfig settings;
settings.request_defaults.headers.set("User-Agent", "MyApp/1.0");
client->set_config(std::move(settings));
```

`RequestContext` merges headers by case-insensitive name in this order:
explicit request headers, profile configuration headers, client request defaults.
Each lower layer fills only missing names. Request modifiers run after this merge
and can inspect the final headers. Defaults are snapshotted for each request and
are not copied into `ParserConfig` or exported as authentication. Existing contexts
see updated defaults on their next request.

Transport implementations override `apply_client_config` instead of `set_config`;
the common nonvirtual setter publishes request defaults independently of the
backend. Proxy, timeout, retry and TLS behavior remains backend-owned.

## Mirrors

Mirror selection remains in each account configuration. `MirrorSource` accepts
optional mirror and frontend-origin maps keyed by `(parser_id, profile_id)`.
There is no parser-wide origin fallback, including for `main` or unstamped
contexts. Selector and regex overrides retain their existing ownership.

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
reading profile settings. `SourceOverridesManager` publishes the profile maps into the
immutable `MirrorSource` for parser request origins. Existing request contexts keep
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
