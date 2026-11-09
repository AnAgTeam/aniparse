#include "catch_amalgamated.hpp"
#include <aniparse/json/Decode.hpp>

namespace decode_policy_test {
struct Child { int count; };
BOOST_DESCRIBE_STRUCT(Child, (), (count))
struct Model { aniparse::Tolerant<Child> child; int id; };
BOOST_DESCRIBE_STRUCT(Model, (), (child, id))
}

namespace json = aniparse::json;
using decode_policy_test::Model;

TEST_CASE("JSON strictness and diagnostics are independent compile-time policies", "[json-decode]") {
    const auto input = boost::json::parse(R"({"child":{"count":"bad"},"id":7})");
    auto ordinary = json::decode<Model>(input, nullptr);
    static_assert(std::is_same_v<typename decltype(ordinary)::error_type, boost::system::error_code>);
    REQUIRE(ordinary);
    CHECK_FALSE(ordinary->child);

    auto strict = json::decode<Model>(input, json::Strict{});
    static_assert(std::is_same_v<typename decltype(strict)::error_type, boost::system::error_code>);
    REQUIRE_FALSE(strict);
    CHECK(strict.error() == boost::json::try_value_to<Model>(input, json::Strict{}).error());

    auto diagnostic = json::decode<Model>(input, json::Diagnostics{});
    static_assert(std::is_same_v<typename decltype(diagnostic)::error_type, json::DecodeError>);
    REQUIRE(diagnostic);
    CHECK_FALSE(diagnostic->child);

    const auto context = std::tuple{json::Strict{}, std::tuple{json::Diagnostics{}}};
    auto checked = json::decode<Model>(input, std::tie(context));
    static_assert(std::is_same_v<typename decltype(checked)::error_type, json::DecodeError>);
    REQUIRE_FALSE(checked);
    CHECK(checked.error().path == "$.child.count");
    CHECK(checked.error().code == strict.error());

    auto broken = input;
    broken.at("id") = false;
    const auto required = json::decode<Model>(broken, nullptr);
    REQUIRE_FALSE(required);
    CHECK_FALSE(required.error().message().empty());
    CHECK(required.error() == boost::json::try_value_to<Model>(broken).error());
    const auto detailed = json::decode<Model>(broken, json::Diagnostics{});
    REQUIRE_FALSE(detailed);
    CHECK(detailed.error().path == "$.id");
    CHECK(detailed.error().code == required.error());
    // Replay uses the first pass's policy, even in a strict diagnostic build.
    CHECK(json::decode_error<Model>(broken).path == "$.id");
}

TEST_CASE("Default JSON policy follows the library build and permits explicit overrides", "[json-decode]") {
    const auto input = boost::json::parse(R"({"child":{"count":false},"id":7})");
    auto result = json::decode<Model>(input);
#if defined(ANIPARSE_JSON_CHECKS) && ANIPARSE_JSON_CHECKS
    static_assert(std::is_same_v<typename decltype(result)::error_type, json::DecodeError>);
    REQUIRE_FALSE(result);
    CHECK(result.error().path == "$.child.count");
#else
    static_assert(std::is_same_v<typename decltype(result)::error_type, boost::system::error_code>);
    REQUIRE(result);
    CHECK_FALSE(result->child);
#endif
    REQUIRE(json::decode<Model>(input, nullptr));
    REQUIRE(json::decode<Model>(input, json::Diagnostics{}));
    REQUIRE_FALSE(json::decode<Model>(input, json::Strict{}));
}
