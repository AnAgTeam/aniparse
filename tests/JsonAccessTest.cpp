/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/json/Json.hpp>

#include <boost/json.hpp>

#include <string>

namespace json = aniparse::json;

namespace {
boost::json::value sample() {
    return boost::json::parse(R"({
        "title": "Berserk",
        "id": 4242,
        "big_id": 9007199254740993,
        "score": 9.5,
        "count": 10,
        "adult": true,
        "safe": false,
        "cover": { "default": "https://cdn/x.jpg" },
        "genres": [ { "name": "Seinen" }, { "name": "Action" } ],
        "wrong_object": 7,
        "wrong_array": "nope"
    })");
}
} // namespace

TEST_CASE("str reads a string field, empty on miss or wrong type") {
    boost::json::value doc = sample();
    const boost::json::object& root = doc.as_object();
    CHECK(json::str(root, "title") == "Berserk");
    CHECK(json::str(root, "missing").empty());
    CHECK(json::str(root, "id").empty()); // present but not a string
}

TEST_CASE("str preserves the full length past an embedded NUL") {
    // Built programmatically: a raw NUL is not valid inside JSON text, so a
    // parsed literal cannot carry one. The three-byte string exercises that str
    // copies the whole value rather than stopping at the NUL.
    const std::string embedded("a\0b", 3);
    boost::json::object root;
    root["nul"] = boost::json::string_view(embedded.data(), embedded.size());
    CHECK(json::str(root, "nul") == embedded);
}

TEST_CASE("integer coerces int, uint, and double; 0 on miss or wrong type") {
    boost::json::value doc = sample();
    const boost::json::object& root = doc.as_object();
    CHECK(json::integer(root, "id") == 4242);
    CHECK(json::integer(root, "big_id") == 9007199254740993LL); // not truncated to 32-bit
    CHECK(json::integer(root, "score") == 9);                   // double truncated
    CHECK(json::integer(root, "missing") == 0);
    CHECK(json::integer(root, "title") == 0); // present but not numeric
}

TEST_CASE("number coerces double and integers; 0.0 on miss or wrong type") {
    boost::json::value doc = sample();
    const boost::json::object& root = doc.as_object();
    CHECK(json::number(root, "score") == Catch::Approx(9.5));
    CHECK(json::number(root, "count") == Catch::Approx(10.0));
    CHECK(json::number(root, "missing") == Catch::Approx(0.0));
    CHECK(json::number(root, "title") == Catch::Approx(0.0));
}

TEST_CASE("boolean reads a bool, false on miss or wrong type") {
    boost::json::value doc = sample();
    const boost::json::object& root = doc.as_object();
    CHECK(json::boolean(root, "adult") == true);
    CHECK(json::boolean(root, "safe") == false);
    CHECK(json::boolean(root, "missing") == false);
    CHECK(json::boolean(root, "id") == false); // present but not a bool
}

TEST_CASE("object_field and array_field return typed pointers, nullptr otherwise") {
    boost::json::value doc = sample();
    const boost::json::object& root = doc.as_object();

    const boost::json::object* cover = json::object_field(root, "cover");
    REQUIRE(cover != nullptr);
    CHECK(json::str(*cover, "default") == "https://cdn/x.jpg");

    const boost::json::array* genres = json::array_field(root, "genres");
    REQUIRE(genres != nullptr);
    CHECK(genres->size() == 2);

    CHECK(json::object_field(root, "missing") == nullptr);
    CHECK(json::array_field(root, "missing") == nullptr);
    CHECK(json::object_field(root, "wrong_object") == nullptr); // present but not an object
    CHECK(json::array_field(root, "wrong_array") == nullptr);   // present but not an array
}

TEST_CASE("pointer overloads short-circuit on nullptr to the absent fallback") {
    const boost::json::object* none = nullptr;
    CHECK(json::str(none, "any").empty());
    CHECK(json::integer(none, "any") == 0);
    CHECK(json::number(none, "any") == Catch::Approx(0.0));
    CHECK(json::boolean(none, "any") == false);
    CHECK(json::object_field(none, "any") == nullptr);
    CHECK(json::array_field(none, "any") == nullptr);
}

TEST_CASE("lookups chain through the pointer overload without an intermediate check") {
    boost::json::value doc = sample();
    const boost::json::object& root = doc.as_object();

    // Present path: root -> cover -> default.
    CHECK(json::str(json::object_field(root, "cover"), "default") == "https://cdn/x.jpg");

    // Missing intermediate: root -> nope (absent) -> default yields the fallback,
    // no null deref despite skipping the check on the intermediate object.
    CHECK(json::str(json::object_field(root, "nope"), "default").empty());
    CHECK(json::object_field(json::object_field(root, "nope"), "deeper") == nullptr);
}
