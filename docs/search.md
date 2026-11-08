# Search capabilities and request values

`SearchCapabilities` describes an endpoint. Its `supported_filters` map contains
`SearchFilterInfo`: a source-provided `name` and `SearchFilterCapabilities`
(text, integer/double/absolute/relative interval, selection, or boolean capabilities). The request is independent: `SearchRequestQuery` holds
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
`catalog/Search.hpp` for both, the key vocabulary, and validation functions.

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

## Fractional intervals and UI steps

`DoubleInterval` is `Interval<double>`; `DoubleIntervalCapabilities` is
`IntervalCapabilities<double>`. Use them for fractional ratings, lengths or
other finite numeric quantities. Integer filters retain `IntInterval`.
Capabilities describe the source, while a query contains only chosen bounds:

```cpp
SearchFilterInfo rating{
    .name = "Rating",
    .capabilities = DoubleIntervalCapabilities{
        .min = 0.0, .max = 10.0, .step = 0.1}
};
DoubleInterval requested{.from = 8.15, .to = 9.5};
IntIntervalCapabilities pages{.min = 1, .max = 1000, .step = 10};
```

Both bounds are inclusive. An absent `from` or `to` leaves that end open;
absence is different from zero. Exact values use equal bounds. A query must
use the declared interval type; integer endpoints are not automatically
promoted into a different variant.

`step` is an **optional UI hint**, not an API restriction. It suggests how far
a slider or stepper moves. It belongs only to capabilities, never to the
query. An absent hint leaves the increment to the consumer. Provided hints
must be positive and finite; consumers should ignore invalid hints. For a
bounded slider, consumers may use `min` as the snapping origin and clamp to
`max`, even if the final segment is shorter. Manual entry must not be rounded
to the suggested grid.

In the example, `8.15` is valid even though the suggested step is `0.1`, and
an integer page count of `23` is valid despite a step of `10`.
`validate_query` checks the value's type, exclusion support, bound order and
allowed limits. It also rejects NaN and either infinity in double query
bounds, including when the capability has no limits. It does **not** check
step multiples or normalize values. Capability bounds themselves are the
parser's responsibility: publish finite, ordered bounds.

If an API accepts only a strict grid, the **parser** must validate that extra
constraint after `validate_query`, before making a request. Document its
origin and increment; use a deliberate tolerance or exact decimal units for
floating-point comparisons. Do not infer such a restriction from `step`.

The same capabilities template serves time intervals. Its step type is the
difference between endpoints (`decltype(T{} - T{})`): numeric for integers
and doubles, a duration for absolute times and relative durations. Consumers
may ignore hints for controls that do not use them.

The CLI represents fractional ranges as `N`, `A..B`, `A..` or `..B`, for example
`--filter rating=8.15..9.5`. The two-dot separator allows signed values and
scientific notation without confusing a minus sign with a separator. JSON
capabilities use type `double` and optional numeric `min`, `max`, `step` fields.
Existing integer CLI range syntax is unchanged.
