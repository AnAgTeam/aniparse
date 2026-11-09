# JSON response models {#json-models}

Keep a source's wire models beside its parser. Describe only the fields it uses
with Boost.Describe and convert the response with `boost::json::try_value_to`.
Unknown members are ignored. Models own their strings and containers, so the
JSON document can be released after conversion. Shared domain models need no
JSON dependency.

```cpp
#include <aniparse/json/Conversion.hpp>
#include <boost/describe.hpp>

namespace source {
struct Author {
    std::string name;
};
BOOST_DESCRIBE_STRUCT(Author, (), (name))

struct Book {
    int id;
    std::string title;
    std::optional<std::string> next_cursor;
    aniparse::Tolerant<Author> author;
};
BOOST_DESCRIBE_STRUCT(Book, (), (id, title, next_cursor, author))
}

auto book = boost::json::try_value_to<source::Book>(document);
if (!book) {
    // Map the error to UnexpectedResponse at the parser's response boundary.
}
```

| Field | Missing or null | Invalid present value |
| --- | --- | --- |
| `T` | Error for a non-nullable required type | Error |
| `std::optional<T>` | Empty by default | Error |
| `aniparse::Tolerant<T>` | Empty by default | Empty in normal conversion; error with `json::Strict` |

For a missing described member Boost leaves its default initialization in place.
Thus an optional-like member with an explicit nonempty initializer retains that
value when absent; explicit null clears it. A required `int id = 5` still fails
when `id` is missing. Use empty initialization for optional response fields unless
the source explicitly defines a fallback.

`Tolerant<T>` is an optional value with `has_value`, `value`, `value_or`, `reset`,
dereference and arrow access. It stores no diagnostic and catches no exceptions.
Its JSON converter suppresses errors returned by the inner `try_value_to<T>`;
allocation failures are not suppressed. Custom converters should return conversion
errors deliberately. Numeric and other ordinary conversions follow Boost.JSON's
rules; this layer does not add coercions or domain validation.

## Test the same models strictly

```cpp
auto checked = boost::json::try_value_to<source::Book>(
    document, aniparse::json::Strict{});
```

Strict mode propagates the original conversion error through nested models,
containers and tolerant wrappers. It still accepts absent/null optional fields.
Use fixture assertions for fields expected to be present: strict conversion alone
cannot detect a field that disappeared. Test deliberately damaged fixtures in both
modes. The policy is explicit, with no debug/release behavior switch.

Contexts are forwarded to inner conversions, including custom `tag_invoke`
converters. Combine strict mode and other Boost conversion contexts in a
`std::tuple`; their order does not change the tolerance policy.

## Choose the unit of loss

```cpp
aniparse::Tolerant<std::vector<Author>> authors; // an error drops the whole list
std::vector<aniparse::Tolerant<Author>> credits; // an error empties one slot
```

The second form preserves the input length and ordering; it does not automatically
filter empty entries. Filter them explicitly when appropriate:

```cpp
auto names = credits
    | std::views::filter([](const auto& credit) { return credit.has_value(); })
    | std::views::transform([](const auto& credit) -> const std::string& {
        return credit->name;
    });
```

This range borrows the decoded model. The model itself owns its data. Required
identities, pagination and content used for reading/playback should not silently
disappear. An empty required array is valid; a missing or malformed one is not.
Perform source-specific validation, such as supported media kinds and positive
IDs, after structural conversion.

## Existing node views

`json::View` is **potentially deprecated**, pending evaluation of response models
in real parsers. It remains supported without compiler deprecation warnings.
Its ownership, selectors and range behavior are unchanged. Use model conversion
for the new experiment; ad hoc field access remains available.
