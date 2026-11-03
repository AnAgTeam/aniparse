# Search capabilities and request values

`SearchCapabilities` describes an endpoint. Its `supported_filters` map contains
`SearchFilterInfo`: a source-provided `name` and `SearchFilterCapabilities`
(text, integer/absolute/relative interval, selection, or boolean capabilities). The request is independent: `SearchRequestQuery` holds
search text and a `SearchFilters` map of `SearchFilterValue` values.

Filter keys remain stable query identities. Names are presentation only, independent
of option labels. A custom filter must supply a nonempty name in the source's
language. Canonical `search_keys` may omit the name; consumers prefer their own
localized canonical title, then the source name, then a readable key as a defensive
fallback. This lets a CLI display source names without forcing an app to use the
source's language for canonical headings. Names never enter query validation or
serialization of selected values.

The pure value declarations live in `catalog/SearchQuery.hpp`; capability
metadata lives in `catalog/SearchCapabilities.hpp`. Existing callers can include
`types/Search.hpp` for both, the key vocabulary, and validation functions.

```cpp
SelectionCapabilities genres{
    .options = {{"drama", {.name = "Drama"}}},
    .open_vocabulary = true,
    .supports_exclusion = true,
};
SearchCapabilities capabilities{
    .supported_filters = {{"genre", {.name = "Genres", .capabilities = std::move(genres)}}},
};
SearchRequestQuery query{
    .filters = {{"genre", SelectionValue{
        .included = {"drama", "custom-genre"},
        .excluded = {"another-genre"},
    }}},
};
auto errors = validate_query(capabilities, query, GetFilters{});
```

`SelectionCapabilities::options` provides display labels and suggested tokens.
`open_vocabulary` independently controls whether other tokens are valid. Empty
options never imply openness: an empty closed axis accepts no nonempty selection;
an open axis can provide options while accepting custom tokens. All tokens stay
opaque and round-trip unchanged between tags, suggestions and queries.

`supports_exclusion` is the axis default, including for custom tokens. A known
`SelectionOption` may override it with an explicit true or false; an absent
option override inherits the axis default. `multiple = false` permits at most
one token across the included and excluded lists. Query values contain no labels,
allowed options, or multiplicity settings. Empty selection values make no choice;
empty tokens, repeated tokens, and a token both included and excluded are invalid.

Interval capabilities hold allowed `min`/`max` endpoints and exclusion support.
Interval values hold requested `from`/`to` endpoints and actual inversion. Every
supplied endpoint must lie within the declared bounds. Absolute and relative time
intervals retain distinct types. Text and boolean capabilities likewise describe
supported inversion; their values carry the caller's actual choice.

`validate_search_query` checks values against capabilities using explicit type
mappings, without relying on matching variant indices. `validate_query` also
checks sorting. Network operations still return the existing request errors;
the caller may await `load_search_capabilities()` once and validate locally before calling
`search()`. Dynamic options may still be fetched and cached, and autocomplete
continues through `suggest()` with the existing filter-key vocabulary.
