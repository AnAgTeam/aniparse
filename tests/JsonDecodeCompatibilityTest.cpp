#include "catch_amalgamated.hpp"
#include "JsonDecodeTest.hpp"
#include <boost/variant2/variant.hpp>
#include <filesystem>
#include <map>
#include <set>

namespace compatibility_test {
struct Name { std::string name; };
BOOST_DESCRIBE_STRUCT(Name, (), (name))
struct Identity { int id; };
BOOST_DESCRIBE_STRUCT(Identity, (), (id))
struct Alternatives {
    boost::variant2::variant<Name, Identity> choice;
    bool after;
};
BOOST_DESCRIBE_STRUCT(Alternatives, (), (choice, after))

// Public conversion traits can override the shape suggested by C++ members.
struct NullModel { int id = 7; };
BOOST_DESCRIBE_STRUCT(NullModel, (), (id))
struct DisabledTuple { int id; };
BOOST_DESCRIBE_STRUCT(DisabledTuple, (), (id))
struct DynamicSequence : std::vector<int> {};
using FixedMap = std::array<std::pair<std::string, int>, 2>;

// Boost recognizes a map by its iterator value and insertion contract; nested
// key_type/mapped_type aliases and emplace(key, value) are not required.
class Map {
    std::map<std::string, int> entries_;
public:
    using value_type = std::pair<const std::string, int>;
    using iterator = std::map<std::string, int>::iterator;
    iterator begin() { return entries_.begin(); }
    iterator end() { return entries_.end(); }
    auto emplace(value_type value) { return entries_.emplace(std::move(value)); }
    iterator insert(iterator hint, value_type value) { return entries_.insert(hint, std::move(value)); }
    bool operator==(const Map&) const = default;
};

struct Throws { int unused; };
BOOST_DESCRIBE_STRUCT(Throws, (), (unused))
Throws tag_invoke(boost::json::value_to_tag<Throws>, const boost::json::value& input) {
    if (input.is_null()) throw std::bad_alloc();
    if (input.is_bool()) throw boost::system::system_error(boost::json::error::not_number);
    throw std::runtime_error("custom conversion failed");
}
} // namespace compatibility_test

namespace std {
template<> struct tuple_size<compatibility_test::DisabledTuple> : integral_constant<size_t, 1> {};
template<> struct tuple_size<compatibility_test::DynamicSequence> : integral_constant<size_t, 2> {};
}
namespace boost::json {
template<> struct is_null_like<compatibility_test::NullModel> : std::true_type {};
template<> struct is_tuple_like<compatibility_test::DisabledTuple> : std::false_type {};
template<> struct is_tuple_like<compatibility_test::DynamicSequence> : std::false_type {};
template<> struct is_map_like<compatibility_test::FixedMap> : std::true_type {};
}

namespace json = aniparse::json;
using namespace compatibility_test;

template<class T>
void compare_conversion(const boost::json::value& input) {
    const auto reference = boost::json::try_value_to<T>(input);
    const auto decoded = decode_test::diagnose<T>(input);
    REQUIRE(decoded.has_value() == reference.has_value());
    if (reference) {
        CHECK(*decoded == *reference);
    } else {
        CHECK(decoded.error().code == reference.error());
    }
}

TEST_CASE("Diagnostic conversion agrees with Boost on ordinary values and containers", "[json-decode]") {
    for (const auto* text : {"null", "true", "123", "-1", "1.5", "1.0", "128", "1e100",
            "\"word\"", "{}", "[]", "[1,2]", "[1,\"x\"]", "[1,{}]",
            R"({"a":1,"b":2})", R"({"a":1,"b":false})", R"([["a",1],["a",2]])"}) {
        CAPTURE(text);
        const auto input = boost::json::parse(text);
        compare_conversion<int>(input);
        compare_conversion<unsigned char>(input);
        compare_conversion<double>(input);
        compare_conversion<bool>(input);
        compare_conversion<std::string>(input);
        compare_conversion<std::optional<int>>(input);
        compare_conversion<std::vector<bool>>(input);
        compare_conversion<std::vector<int>>(input);
        compare_conversion<std::array<int, 2>>(input);
        compare_conversion<std::tuple<int, std::string>>(input);
        compare_conversion<std::pair<const int, int>>(input);
        compare_conversion<std::set<int>>(input);
        compare_conversion<std::map<std::string, int>>(input);
        compare_conversion<std::multimap<std::string, int>>(input);
        compare_conversion<boost::variant2::variant<int, std::string>>(input);
        compare_conversion<std::filesystem::path>(input);
    }
}

TEST_CASE("Diagnostic conversion respects Boost category traits and container contracts", "[json-decode]") {
    auto null = decode_test::diagnose<NullModel>(boost::json::value(nullptr));
    REQUIRE(null);
    CHECK(null->id == boost::json::try_value_to<NullModel>(boost::json::value(nullptr))->id);
    auto wrong_null = decode_test::diagnose<NullModel>(boost::json::object{{"id", 1}});
    REQUIRE_FALSE(wrong_null);
    CHECK(wrong_null.error().code == boost::json::error::not_null);
    CHECK(wrong_null.error().expected == "null");

    const boost::json::value object = boost::json::object{{"id", 42}};
    auto described = decode_test::diagnose<DisabledTuple>(object);
    REQUIRE(described);
    CHECK(described->id == boost::json::try_value_to<DisabledTuple>(object)->id);
    auto wrong_object = decode_test::diagnose<DisabledTuple>(boost::json::array{42});
    REQUIRE_FALSE(wrong_object);
    CHECK(wrong_object.error().code == boost::json::error::not_object);

    compare_conversion<DynamicSequence>(boost::json::array{1, 2, 3});
    compare_conversion<Map>(boost::json::object{{"a", 1}, {"b", 2}});
    compare_conversion<Map>(boost::json::object{{"a", "bad"}});
    compare_conversion<FixedMap>(boost::json::object{{"a", 1}, {"b", 2}});
    compare_conversion<FixedMap>(boost::json::object{{"a", 1}});
    const auto missing = decode_test::diagnose<FixedMap>(boost::json::object{{"a", 1}});
    REQUIRE_FALSE(missing);
    CHECK(missing.error().expected == "object of 2 members");
    auto fixed = decode_test::diagnose<FixedMap>(boost::json::object{{"a", 1}, {"b", false}});
    REQUIRE_FALSE(fixed);
    CHECK(fixed.error().path == "$.b");
}

TEST_CASE("All Boost variant-like probes discard speculative diagnostics", "[json-decode]") {
    auto input = boost::json::parse(R"({"choice":{"name":false,"id":7},"after":true})");
    auto result = decode_test::diagnose<Alternatives>(input);
    REQUIRE(result);
    CHECK(boost::variant2::get<Identity>(result->choice).id == 7);
    input.at("after") = "bad";
    result = decode_test::diagnose<Alternatives>(input);
    REQUIRE_FALSE(result);
    CHECK(result.error().path == "$.after");
    CHECK(result.error().code == boost::json::try_value_to<Alternatives>(input).error());
    input.at("choice") = false;
    result = decode_test::diagnose<Alternatives>(input);
    REQUIRE_FALSE(result);
    CHECK(result.error().path == "$.choice");
    CHECK(result.error().code == boost::json::try_value_to<Alternatives>(input).error());
}

TEST_CASE("Repeated JSON text keys use the Boost parser's final value", "[json-decode]") {
    const auto input = boost::json::parse(R"({"id":"bad","id":9,"extra":false})");
    auto result = decode_test::diagnose<Identity>(input);
    REQUIRE(result);
    CHECK(result->id == 9);
    CHECK(result->id == boost::json::try_value_to<Identity>(input)->id);
    auto invalid = decode_test::diagnose<Identity>(boost::json::parse(R"({"id":9,"id":"bad"})"));
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().path == "$.id");
}

TEST_CASE("Throwing custom converters retain Boost exception translation", "[json-decode]") {
    for (const boost::json::value input : {boost::json::value(true), boost::json::value(42)}) {
        const auto reference = boost::json::try_value_to<Throws>(input);
        const auto decoded = decode_test::diagnose<Throws>(input);
        REQUIRE_FALSE(decoded);
        CHECK(decoded.error().code == reference.error());
        CHECK(decoded.error().path == "$");
    }
    CHECK_THROWS_AS(decode_test::diagnose<Throws>(boost::json::value(nullptr)), std::bad_alloc);
}
