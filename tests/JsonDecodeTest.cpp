#include "catch_amalgamated.hpp"
#include "JsonDecodeTest.hpp"
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <thread>

namespace decode_test {
struct Author { std::string name; };
BOOST_DESCRIBE_STRUCT(Author, (), (name))
struct Model {
    aniparse::Tolerant<Author> author;
    std::vector<Author> authors;
    int id = 5;
    std::optional<int> optional = 7;
};
BOOST_DESCRIBE_STRUCT(Model, (), (author, authors, id, optional))
struct Tree { std::vector<Tree> children; int id; };
BOOST_DESCRIBE_STRUCT(Tree, (), (children, id))
struct Base { int id; };
BOOST_DESCRIBE_STRUCT(Base, (), (id))
struct Derived : Base { std::string title; };
BOOST_DESCRIBE_STRUCT(Derived, (Base), (title))
struct Alternatives {
    std::variant<Author, Base> choice;
    std::string after;
};
BOOST_DESCRIBE_STRUCT(Alternatives, (), (choice, after))

struct Prefix { std::string text; };
// A described type with an explicitly different contextual wire representation.
struct Custom { std::string name; };
BOOST_DESCRIBE_STRUCT(Custom, (), (name))
boost::system::result<Custom> tag_invoke(boost::json::try_value_to_tag<Custom>,
    const boost::json::value& input, const Prefix& prefix) {
    auto value = boost::json::try_value_to<std::string>(input);
    if (!value) {
        return {boost::system::in_place_error, value.error()};
    }
    return Custom{prefix.text + *value};
}
struct MoveOnly {
    explicit MoveOnly(int number) : value(std::make_unique<int>(number)) {}
    std::unique_ptr<int> value;
};
boost::system::result<MoveOnly> tag_invoke(boost::json::try_value_to_tag<MoveOnly>,
    const boost::json::value& input) {
    auto value = boost::json::try_value_to<int>(input);
    if (!value) {
        return {boost::system::in_place_error, value.error()};
    }
    return MoveOnly{*value};
}
struct Throws {};
boost::system::result<Throws> tag_invoke(boost::json::try_value_to_tag<Throws>, const boost::json::value&) {
    throw std::bad_alloc();
}
struct PlainCustom { int id; };
BOOST_DESCRIBE_STRUCT(PlainCustom, (), (id))
PlainCustom tag_invoke(boost::json::value_to_tag<PlainCustom>, const boost::json::value& input) {
    return {boost::json::value_to<int>(input)};
}
struct Forwarded { std::vector<Author> authors; };
BOOST_DESCRIBE_STRUCT(Forwarded, (), (authors))
template<class FullContext>
boost::system::result<Forwarded> tag_invoke(boost::json::try_value_to_tag<Forwarded>,
    const boost::json::value& input, const Prefix&, const FullContext& full) {
    auto authors = boost::json::try_value_to<std::vector<Author>>(input, full);
    if (!authors) {
        return {boost::system::in_place_error, authors.error()};
    }
    return Forwarded{std::move(*authors)};
}
struct CountCalls { int* calls; };
struct Counted { int value; };
boost::system::result<Counted> tag_invoke(boost::json::try_value_to_tag<Counted>,
    const boost::json::value& input, const CountCalls& context) {
    ++*context.calls;
    auto value = boost::json::try_value_to<int>(input);
    if (!value) {
        return {boost::system::in_place_error, value.error()};
    }
    return Counted{*value};
}
} // namespace decode_test

using namespace decode_test;
namespace json = aniparse::json;

TEST_CASE("Error-branch diagnostics reproduce nested failures with the same context", "[json-decode]") {
    const auto input = boost::json::parse(R"({"author":{"name":42},"authors":[{"name":false}],"id":7})");
    auto normal = boost::json::try_value_to<Model>(input);
    REQUIRE_FALSE(normal.has_value());
    auto error = json::decode_error<Model>(input);
    CHECK(error.path == "$.authors[0].name");
    CHECK(error.code == normal.error());
    CHECK(error.message() == decode_test::diagnose<Model>(input).error().message());
    const auto context = std::tuple{Prefix{"Dr. "}, std::tuple{json::Strict{}}};
    const auto strict = boost::json::try_value_to<Model>(input, context);
    REQUIRE_FALSE(strict.has_value());
    CHECK(json::decode_error<Model>(input, context).path == "$.author.name");
    using Target = std::vector<aniparse::Tolerant<Custom>>;
    const auto custom = boost::json::parse(R"(["A",42])");
    REQUIRE_FALSE(boost::json::try_value_to<Target>(custom, context).has_value());
    CHECK(json::decode_error<Target>(custom, context).path == "$[1]");
    auto owned = json::decode_error<Model>(boost::json::parse(R"({"authors":[],"id":"bad"})"));
    CHECK(owned.path == "$.id");
}

TEST_CASE("Lazy diagnostics repeat conversion only after a fatal error", "[json-decode]") {
    int calls = 0;
    const CountCalls context{&calls};
    const auto convert = [&]<class T>(const boost::json::value& input) -> aniparse::Result<T, json::DecodeError> {
        auto result = boost::json::try_value_to<T>(input, context);
        if (!result) {
            return aniparse::unexpected(json::decode_error<T>(input, context));
        }
        return std::move(*result);
    };
    auto valid = convert.template operator()<Counted>(boost::json::value(7));
    REQUIRE(valid);
    CHECK(valid->value == 7);
    CHECK(calls == 1);
    calls = 0;
    auto ignored = convert.template operator()<aniparse::Tolerant<Counted>>(boost::json::value(false));
    REQUIRE(ignored);
    CHECK_FALSE(*ignored);
    CHECK(calls == 1);
    calls = 0;
    auto invalid = convert.template operator()<Counted>(boost::json::value(false));
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().path == "$");
    CHECK(calls == 2);
    CHECK_THROWS_AS(json::decode_error<int>(boost::json::value(7)), std::logic_error);
}

TEST_CASE("JSON decoding locates the first bad field and owns its diagnostic", "[json-decode]") {
    auto result = decode_test::diagnose<Model>(boost::json::parse(
        R"({"authors":[{"name":"A"},{"name":42}],"id":"also wrong"})"));
    REQUIRE_FALSE(result);
    CHECK(result.error().path == "$.authors[1].name");
    CHECK(result.error().expected == "string");
    CHECK(result.error().actual == "integer");
    CHECK(result.error().code == boost::json::error::not_string);
    CHECK(result.error().message() == "$.authors[1].name: expected string, got integer (value is not a string)");
    auto copy = result.error();
    result = Model{};
    CHECK(copy.path == "$.authors[1].name");
}

TEST_CASE("JSON decoding preserves optional initializers and rejects missing required fields", "[json-decode]") {
    auto input = boost::json::parse(R"({"authors":[],"id":1,"ignored":42})");
    auto result = decode_test::diagnose<Model>(input);
    REQUIRE(result);
    CHECK(result->id == 1);
    CHECK(result->optional == 7);
    input.as_object()["optional"] = nullptr;
    result = decode_test::diagnose<Model>(input);
    REQUIRE(result);
    CHECK_FALSE(result->optional);
    input.as_object().erase("id");
    result = decode_test::diagnose<Model>(input);
    REQUIRE_FALSE(result);
    CHECK(result.error().path == "$.id");
    CHECK(result.error().actual == "missing");
    CHECK(result.error().message().find("required field is missing") != std::string::npos);
    auto derived = decode_test::diagnose<Derived>(boost::json::parse(R"({"id":"wrong","title":"A"})"));
    REQUIRE_FALSE(derived);
    CHECK(derived.error().path == "$.id");
}

TEST_CASE("JSON diagnostics preserve scalar conversion rules", "[json-decode]") {
    for (const auto* text : {"null", "true", "\"42\"", "1.5", "2147483648", "-2147483649", "[]", "{}"}) {
        CAPTURE(text);
        auto input = boost::json::parse(text);
        auto boost_result = boost::json::try_value_to<int>(input);
        auto result = decode_test::diagnose<int>(input);
        REQUIRE_FALSE(result);
        CHECK(result.error().code == boost_result.error());
        CHECK(result.error().path == "$");
    }
    for (const auto* text : {"0", "-2147483648", "2147483647", "42.0"}) {
        auto input = boost::json::parse(text);
        auto result = decode_test::diagnose<int>(input);
        REQUIRE(result);
        CHECK(*result == *boost::json::try_value_to<int>(input));
    }
    auto optional = decode_test::diagnose<std::optional<std::string>>(boost::json::value(42));
    REQUIRE_FALSE(optional);
    CHECK(optional.error().expected == "string");
    CHECK(decode_test::diagnose<std::optional<int>>(boost::json::value(nullptr)));
    CHECK(decode_test::diagnose<boost::json::value>(boost::json::parse(R"({"a":[]})")));
}

TEST_CASE("JSON tuples pairs and homogeneous arrays use JSON indices", "[json-decode]") {
    auto tuple = decode_test::diagnose<std::tuple<int, std::string, bool>>(boost::json::parse(R"([1,42,true])"));
    REQUIRE_FALSE(tuple);
    CHECK(tuple.error().path == "$[1]");
    auto pair = decode_test::diagnose<std::pair<std::string, int>>(boost::json::parse(R"(["A","wrong"])"));
    REQUIRE_FALSE(pair);
    CHECK(pair.error().path == "$[1]");
    auto array = decode_test::diagnose<std::array<int, 3>>(boost::json::parse("[1,2,null]"));
    REQUIRE_FALSE(array);
    CHECK(array.error().path == "$[2]");
    auto size = decode_test::diagnose<std::array<int, 3>>(boost::json::parse("[1,2]"));
    REQUIRE_FALSE(size);
    CHECK(size.error().path == "$");
    CHECK(size.error().expected == "array of 3 elements");
    CHECK(size.error().actual == "array of 2 elements");
    auto moved = decode_test::diagnose<std::pair<MoveOnly, int>>(boost::json::parse("[7,8]"));
    REQUIRE(moved);
    CHECK(*moved->first.value == 7);
    CHECK(moved->second == 8);
    auto moved_root = decode_test::diagnose<const MoveOnly&>(boost::json::value(9));
    REQUIRE(moved_root);
    CHECK(*moved_root->value == 9);
    CHECK(decode_test::diagnose<std::deque<int>>(boost::json::parse("[1,2]")));
    auto set = decode_test::diagnose<std::set<int>>(boost::json::parse("[2,1,2]"));
    REQUIRE(set);
    CHECK(set->size() == 2);
}

TEST_CASE("JSON object keys are escaped unambiguously", "[json-decode]") {
    for (const std::string key : {"a.b", "", "0", "quote\"slash\\\n", "ключ"}) {
        boost::json::object input;
        input[key] = boost::json::array{1, "wrong"};
        auto result = decode_test::diagnose<std::map<std::string, std::vector<int>>>(input);
        REQUIRE_FALSE(result);
        CHECK(result.error().path == "$[" + boost::json::serialize(boost::json::string(key)) + "][1]");
    }
}

TEST_CASE("Tolerant diagnostic failures are discarded or propagated by the selected policy", "[json-decode]") {
    auto input = boost::json::parse(R"({"author":{"name":42},"authors":[],"id":1})");
    auto normal = decode_test::diagnose<Model>(input);
    REQUIRE(normal);
    CHECK_FALSE(normal->author);
    auto strict = decode_test::diagnose<Model>(input, json::Strict{});
    REQUIRE_FALSE(strict);
    CHECK(strict.error().path == "$.author.name");
    input.at("id") = "wrong";
    normal = decode_test::diagnose<Model>(input);
    REQUIRE_FALSE(normal);
    CHECK(normal.error().path == "$.id");
    using Each = std::vector<aniparse::Tolerant<Author>>;
    auto each = decode_test::diagnose<Each>(boost::json::parse(R"([{"name":42},{"name":"A"}])"));
    REQUIRE(each);
    CHECK_FALSE((*each)[0]);
    REQUIRE((*each)[1]);
    CHECK((*each)[1]->name == "A");
}

TEST_CASE("Variant probes leave no stale diagnostic when another branch succeeds", "[json-decode]") {
    auto input = boost::json::parse(R"({"choice":{"name":42,"id":7},"after":"ok"})");
    auto result = decode_test::diagnose<Alternatives>(input, json::Strict{});
    REQUIRE(result);
    CHECK(std::get<Base>(result->choice).id == 7);
    input.at("after") = false;
    result = decode_test::diagnose<Alternatives>(input);
    REQUIRE_FALSE(result);
    CHECK(result.error().path == "$.after");
    input.at("choice") = false;
    result = decode_test::diagnose<Alternatives>(input);
    REQUIRE_FALSE(result);
    CHECK(result.error().path == "$.choice");
    CHECK(result.error().code == boost::json::error::exhausted_variants);
}

TEST_CASE("JSON diagnostic contexts preserve custom conversion precedence and strict mode", "[json-decode]") {
    const Prefix prefix{"Dr. "};
    const json::Strict strict;
    using Target = std::vector<aniparse::Tolerant<Custom>>;
    auto input = boost::json::parse(R"(["A",42])");
    auto normal = decode_test::diagnose<Target>(input, prefix);
    REQUIRE(normal);
    REQUIRE((*normal)[0]);
    CHECK((*normal)[0]->name == "Dr. A");
    CHECK_FALSE((*normal)[1]);
    for (bool reverse : {false, true}) {
        auto result = reverse ? decode_test::diagnose<Target>(input, std::tie(prefix, strict))
                              : decode_test::diagnose<Target>(input, std::tie(strict, prefix));
        REQUIRE_FALSE(result);
        CHECK(result.error().path == "$[1]");
        CHECK(result.error().expected == "string");
    }
    CHECK_FALSE(decode_test::diagnose<Target>(input, std::tuple{prefix, std::tuple{strict}}));
    auto plain = decode_test::diagnose<std::vector<PlainCustom>>(boost::json::parse("[1,2]"));
    REQUIRE(plain);
    CHECK((*plain)[1].id == 2);
    auto empty_context = decode_test::diagnose<PlainCustom>(boost::json::value(7), std::tuple{});
    REQUIRE(empty_context);
    CHECK(empty_context->id == 7);
    auto forwarded = decode_test::diagnose<Forwarded>(boost::json::parse(R"([{"name":42}])"), prefix);
    REQUIRE_FALSE(forwarded);
    CHECK(forwarded.error().path == "$[0].name");
    CHECK_THROWS_AS(decode_test::diagnose<aniparse::Tolerant<Throws>>(boost::json::value(1)), std::bad_alloc);
}

TEST_CASE("JSON diagnostic path nodes remain valid through recursion and independent calls", "[json-decode]") {
    boost::json::value input{{"children", boost::json::array{}}, {"id", "wrong"}};
    constexpr int depth = 32;
    std::string path = "$";
    for (int index = 0; index < depth; ++index) {
        input = boost::json::object{{"children", boost::json::array{std::move(input)}}, {"id", 1}};
        path += ".children[0]";
    }
    path += ".id";
    auto result = decode_test::diagnose<Tree>(input);
    REQUIRE_FALSE(result);
    CHECK(result.error().path == path);
    std::array<std::string, 4> paths;
    std::vector<std::jthread> threads;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        threads.emplace_back([&, index] {
            auto decoded = decode_test::diagnose<Tree>(input);
            paths[index] = decoded.error().path;
        });
    }
    threads.clear();
    for (const auto& actual : paths) {
        CHECK(actual == path);
    }
}
