# JSON response models {#json-models}

Keep a source's wire models beside its parser. Describe only the fields it uses
with Boost.Describe and convert the response with `json::decode`.
Unknown members are ignored. Models own their strings and containers, so the
JSON document can be released after conversion. Shared domain models need no
JSON dependency.

```cpp
#include <aniparse/json/Decode.hpp>
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

auto book = aniparse::json::decode<source::Book>(document);
if (!book) {
    // Map book.error().message() to UnexpectedResponse at the response boundary.
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

Repeated names in the described C++ model are rejected at compile time, including
conflicts between inherited members. This is separate from duplicate keys in JSON
text: Boost's parser retains the last value of a repeated key before conversion
starts. Unknown JSON fields remain ignored.

Diagnostic conversion uses Boost's category selection, container insertion and
variant machinery. Public `is_*_like` specializations retain their meaning;
`boost::variant2::variant` receives the same speculative-error handling as
`std::variant`. Unsupported destination types are not enabled by diagnostics:
for example, Boost 1.91's generic conversion rejects `std::tuple<>` and
`std::array<T, 0>`. A dynamic container still accepts an empty JSON array.

## Test the same models strictly

```cpp
auto checked = aniparse::json::decode<source::Book>(
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

## Select the build policy

`Strict` controls whether malformed `Tolerant` values fail conversion.
`Diagnostics` independently enables an owned error path and type details:

```cpp
auto ordinary = aniparse::json::decode<source::Book>(document, nullptr);
auto strict = aniparse::json::decode<source::Book>(document, aniparse::json::Strict{});
auto detailed = aniparse::json::decode<source::Book>(document, aniparse::json::Diagnostics{});
auto checked = aniparse::json::decode<source::Book>(document,
    std::tuple{aniparse::json::Strict{}, aniparse::json::Diagnostics{}});
```

Without `Diagnostics`, the result is `Result<T, boost::system::error_code>` and
conversion uses ordinary Boost without instantiating the diagnostic traversal.
With `Diagnostics`, the error type is `json::DecodeError`. Both errors provide
`message()`, so parser error mapping can use the same code in either mode.

Calls without an explicit context use `json::DefaultContext`. The CMake option
`ANIPARSE_JSON_CHECKS` is OFF by default, selecting ordinary tolerant conversion.
Configure a separate parser-test build with `-DANIPARSE_JSON_CHECKS=ON` to select
`Strict + Diagnostics` for these calls. The option is independent of Debug/Release
and `ANIPARSE_BUILD_TESTS`; tests can run against either production or checked
behavior. The core target publishes the definition to its CMake consumers, so
parser libraries and test executables are compiled with the same policy. Installed
CMake targets also retain their build's setting.

Defining the macro only on a test executable cannot change a precompiled parser.
Rebuild the core, parser libraries and consumers with the selected CMake option;
do not override the definition separately on individual targets. This policy only
affects calls through `json::decode`, including parser response helpers that use
`DefaultContext`. Direct Boost conversion, manual field access and `json::View`
keep their existing behavior.

An explicit context replaces the default. Use `nullptr` to test production
tolerance in a checked build. Compose `json::DefaultContext{}` into a context
tuple alongside a custom converter's settings to preserve the build policy.

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

## Conversion diagnostics

With `json::Diagnostics`, `json::decode<T>` returns `Result<T, json::DecodeError>`.
Its error owns the JSON
path, expected and actual types, and the original Boost error code. The error
remains valid after the input document and conversion stack are destroyed.
Examples of `error().message()`:

```text
$.authors[1].name: expected string, got integer (value is not a string)
$.id: required field is missing (expected signed integer)
$[2]: expected signed integer, got null (not a number)
```

The path starts at `$`. Described fields and ordinary map keys use `.name`;
other keys use a JSON-escaped string, such as `$["author.name"]`. All sequential
containers, including pairs and tuples, use `[index]`. A wrong container size is
reported at the container itself. The first failed required conversion wins;
described fields are visited in their declaration order.

Traversal borrows a linked chain of stack nodes. No path string is assembled on
successful conversion. Built-in scalar leaves create a path node only on failure.
Discarded `Tolerant` failures and rejected variant-like alternatives do not
allocate diagnostics either. If all variant alternatives fail, the error names
the variant's JSON node; it does not collect errors from every alternative.
Allocations needed by the destination model still occur normally.

Existing custom `tag_invoke` converters retain precedence. Their errors are
located at the current conversion boundary. Forwarding the full context into
nested model/container conversions enables diagnostics for those traversals;
manual `object.at(...)` access inside a custom converter cannot automatically
add field names to the path. A converter that speculatively tries alternatives
should keep those probes opaque rather than forwarding diagnostics to them.

String views still borrow the input document; `decode` does not extend its
lifetime. Decode container storage first and derive ranges/spans from that
storage. Destination containers must own their storage.

The simpler `boost::json::try_value_to<T>` entry point remains available through
`json/Conversion.hpp` when only a Boost error code is needed. Both entry points
support the same `Strict` policy.

The implementation uses internal Boost traversal helpers because the public API
does not expose element-visiting hooks. Differential runtime and compile-failure
tests guard this integration when updating Boost. The explicit requirement that
destination containers own their storage still applies.

## Diagnose only after failure

For diagnostics only after a failed ordinary conversion, call `decode_error<T>`
in the error branch with the same parsed JSON and conversion context:

```cpp
auto book = boost::json::try_value_to<source::Book>(document, context);
if (!book) {
    auto error = aniparse::json::decode_error<source::Book>(document, context);
    // Map error.message() to UnexpectedResponse at the response boundary.
}
```

The successful path does not receive a diagnostic context. A failure repeats
model conversion to recover the path; it does not parse the JSON text again.
Custom converters must be repeatable. `decode_error` throws `std::logic_error`
if the second pass succeeds: no failure can be reported for that input/context.
Use the single-pass `decode` when converters cannot safely run twice.

## Benchmark

Enable `ANIPARSE_BUILD_BENCHMARKS` and build `json-decode-benchmark` in Release
mode. It compares `boost::json::try_value_to`, `json::decode`, and ordinary Boost
with `json::decode_error` in its error branch on the same
pre-parsed inputs and models. The CSV output reports median nanoseconds per
conversion, their ratio, and C++ allocation counts. JSON text parsing and error
message formatting are outside the timed region; creating an owned diagnostic
on failure is included. Separate translation units and an observable result
prevent the driver from discarding model construction.

Each case uses nine samples in rotating order after calibration. Successful
cases fail the benchmark if diagnostics add C++ allocations. The counter covers
`operator new` calls, including aligned allocation, but not direct `malloc` calls
inside dependencies. Cases include a homogeneous fixed array, nested response
pages, tuples, ignored tolerant failures, variant retries, and fatal errors.

## Existing node views

`json::View` is **potentially deprecated**, pending evaluation of response models
in real parsers. It remains supported without compiler deprecation warnings.
Its ownership, selectors and range behavior are unchanged. Use model conversion
for the new experiment; ad hoc field access remains available.
