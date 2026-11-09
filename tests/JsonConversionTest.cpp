#include "catch_amalgamated.hpp"
#include <aniparse/json/Conversion.hpp>
#include <boost/describe.hpp>

#include <memory>
#include <new>
#include <string>
#include <tuple>
#include <vector>

namespace conversion_test {
using aniparse::Tolerant;

struct Author {
    std::string name;
};
BOOST_DESCRIBE_STRUCT(Author, (), (name))

struct Model {
    int id = 5;
    std::optional<Author> editor;
    Tolerant<Author> author;
};
BOOST_DESCRIBE_STRUCT(Model, (), (id, editor, author))

struct Authors {
    Tolerant<std::vector<Author>> all;
    std::vector<Tolerant<Author>> each;
};
BOOST_DESCRIBE_STRUCT(Authors, (), (all, each))

struct Nested {
    Tolerant<Model> model;
};
BOOST_DESCRIBE_STRUCT(Nested, (), (model))

struct Prefix {
    std::string text;
};
struct ContextAuthor {
    std::string name;
};
boost::system::result<ContextAuthor> tag_invoke(
    boost::json::try_value_to_tag<ContextAuthor>, const boost::json::value& input,
    const Prefix& prefix) {
    auto name = boost::json::try_value_to<std::string>(input);
    if (!name) {
        return {boost::system::in_place_error, name.error()};
    }
    return ContextAuthor{prefix.text + *name};
}

struct AllocationFailure {};
boost::system::result<AllocationFailure> tag_invoke(
    boost::json::try_value_to_tag<AllocationFailure>, const boost::json::value&) {
    throw std::bad_alloc();
}
} // namespace conversion_test

using namespace conversion_test;
namespace json = aniparse::json;

TEST_CASE("JSON models distinguish required optional and tolerant fields", "[json-conversion]") {
    const auto input = boost::json::parse(R"({"id":7,"author":{"name":"A"},"ignored":true})");
    auto model = boost::json::try_value_to<Model>(input, json::Strict{});
    REQUIRE(model.has_value());
    CHECK(model->id == 7);
    CHECK_FALSE(model->editor);
    REQUIRE(model->author);
    CHECK(model->author->name == "A");

    for (const auto* text : {R"({"id":7})", R"({"id":7,"editor":null,"author":null})"}) {
        auto missing = boost::json::try_value_to<Model>(boost::json::parse(text), json::Strict{});
        REQUIRE(missing.has_value());
        CHECK_FALSE(missing->author);
        CHECK_FALSE(missing->editor);
    }
    // A member initializer is not a default for a missing required JSON field.
    for (const auto* text : {R"({})", R"({"id":null})", R"({"id":"7"})",
                            R"({"id":7,"editor":{"name":42}})"}) {
        auto malformed = boost::json::parse(text);
        CHECK_FALSE(boost::json::try_value_to<Model>(malformed).has_value());
        CHECK_FALSE(boost::json::try_value_to<Model>(malformed, json::Strict{}).has_value());
    }
}

TEST_CASE("Tolerant JSON conversion preserves the inner error in strict mode", "[json-conversion]") {
    for (const auto* text : {R"({"name":42})", R"({})", R"([])", R"(false)"}) {
        const auto input = boost::json::parse(text);
        auto normal = boost::json::try_value_to<Tolerant<Author>>(input);
        REQUIRE(normal.has_value());
        CHECK_FALSE(*normal);
        auto strict = boost::json::try_value_to<Tolerant<Author>>(input, json::Strict{});
        REQUIRE_FALSE(strict.has_value());
        CHECK(strict.error() == boost::json::try_value_to<Author>(input).error());
    }
    CHECK_FALSE(boost::json::value_to<Tolerant<Author>>(boost::json::value(42)));
    CHECK_THROWS(boost::json::value_to<Tolerant<Author>>(boost::json::value(42), json::Strict{}));
}

TEST_CASE("Tolerant collection placement controls which values are discarded", "[json-conversion]") {
    const auto input = boost::json::parse(R"({
        "all":[{"name":"A"},{"name":42},{"name":"B"}],
        "each":[{"name":"A"},{"name":42},{"name":"B"},null]
    })");
    auto authors = boost::json::try_value_to<Authors>(input);
    REQUIRE(authors.has_value());
    CHECK_FALSE(authors->all);
    REQUIRE(authors->each.size() == 4);
    REQUIRE(authors->each[0]);
    CHECK(authors->each[0]->name == "A");
    CHECK_FALSE(authors->each[1]);
    REQUIRE(authors->each[2]);
    CHECK(authors->each[2]->name == "B");
    CHECK_FALSE(authors->each[3]);
    CHECK_FALSE(boost::json::try_value_to<Authors>(input, json::Strict{}).has_value());
}

TEST_CASE("Strict errors propagate through nested tolerant fields and containers", "[json-conversion]") {
    const auto input = boost::json::parse(R"([{"model":{"id":7,"author":{"name":42}}}])");
    auto normal = boost::json::try_value_to<std::vector<Nested>>(input);
    REQUIRE(normal.has_value());
    REQUIRE(normal->front().model);
    CHECK_FALSE(normal->front().model->author);
    CHECK_FALSE(boost::json::try_value_to<std::vector<Nested>>(input, json::Strict{}).has_value());
}

TEST_CASE("Tolerant conversions forward all contexts regardless of tuple order", "[json-conversion]") {
    using Target = Tolerant<std::vector<Tolerant<ContextAuthor>>>;
    const auto input = boost::json::parse(R"(["A",42])");
    const Prefix prefix{"Dr. "};
    auto normal = boost::json::try_value_to<Target>(input, prefix);
    REQUIRE(normal.has_value());
    REQUIRE(*normal);
    REQUIRE((**normal)[0]);
    CHECK((**normal)[0]->name == "Dr. A");
    CHECK_FALSE((**normal)[1]);
    CHECK(boost::json::try_value_to<Target>(input, std::tie(prefix)).has_value());
}

TEST_CASE("Strict contexts compose with custom conversions", "[json-conversion]") {
    const Prefix prefix{"Dr. "};
    const json::Strict strict;
    const auto input = boost::json::parse(R"(["A",42])");
    using Target = Tolerant<std::vector<Tolerant<ContextAuthor>>>;
    CHECK_FALSE(boost::json::try_value_to<Target>(input, std::tie(prefix, strict)).has_value());
    CHECK_FALSE(boost::json::try_value_to<Target>(input, std::tie(strict, prefix)).has_value());
    CHECK_FALSE(boost::json::try_value_to<Target>(input, std::tuple{prefix, std::tuple{strict}}).has_value());
    auto valid = boost::json::try_value_to<Target>(boost::json::parse(R"(["A"])"), std::tie(prefix, strict));
    REQUIRE(valid.has_value());
    REQUIRE(*valid);
    REQUIRE((**valid)[0]);
    CHECK((**valid)[0]->name == "Dr. A");
}

TEST_CASE("Tolerant values own their data and do not catch allocation failures", "[json-conversion]") {
    auto owned = boost::json::value_to<Tolerant<Author>>(boost::json::parse(R"({"name":"owned"})"));
    REQUIRE(owned);
    CHECK(owned->name == "owned");
    owned.reset();
    CHECK_FALSE(owned);
    CHECK_THROWS_AS(owned.value(), std::bad_optional_access);
    Tolerant<std::unique_ptr<int>> move_only(std::make_unique<int>(7));
    auto moved = std::move(move_only).value();
    CHECK(*moved == 7);
    CHECK_THROWS_AS(boost::json::try_value_to<Tolerant<AllocationFailure>>(boost::json::value(1)), std::bad_alloc);
    CHECK_THROWS_AS(boost::json::try_value_to<Tolerant<AllocationFailure>>(boost::json::value(1), json::Strict{}), std::bad_alloc);
}
