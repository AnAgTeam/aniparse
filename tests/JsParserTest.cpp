/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/html/JSParser.hpp>

#include <string>

using namespace aniparse::html;

TEST_CASE("find_json_var extracts an array literal") {
    std::string js = "var x = 1; var fullimg = [\"a\", \"b}c\", 'd'];\nfoo();";
    CHECK(find_json_var("fullimg", js) == R"(["a", "b}c", 'd'])");
}

TEST_CASE("find_json_var extracts a nested object literal") {
    std::string js = "window.data = { \"a\": [1, 2], \"b\": {\"c\": 3} };";
    CHECK(find_json_var("data", js) == R"({ "a": [1, 2], "b": {"c": 3} })");
}

TEST_CASE("find_json_var ignores brackets inside strings and comments") {
    std::string js = "var v = { \"k\": \"}]\", /* } ] */ \"n\": 1 };";
    CHECK(find_json_var("v", js) == R"({ "k": "}]", /* } ] */ "n": 1 })");
}

TEST_CASE("find_json_var matches whole identifiers only") {
    std::string js = "var xfullimg = [9]; var fullimg = [1, 2];";
    CHECK(find_json_var("fullimg", js) == "[1, 2]");
}

TEST_CASE("find_json_var supports quoted keys with a colon") {
    std::string js = "{ \"fullimg\": [\"u1\", \"u2\"] }";
    CHECK(find_json_var("fullimg", js) == R"(["u1", "u2"])");
}

TEST_CASE("find_json_var returns empty when absent, unbalanced or not a literal") {
    // The rvalue overload is deleted, so the searched text must outlive the
    // returned view; bind each string to a named variable.
    std::string absent      = "var other = [1];";
    std::string unbalanced  = "var v = [1, 2, 3";
    std::string not_literal = "var v = 42;";
    CHECK(find_json_var("fullimg", absent).empty());
    CHECK(find_json_var("v", unbalanced).empty());
    CHECK(find_json_var("v", not_literal).empty());
}

TEST_CASE("parse_json_var parses an object literal") {
    std::string js = R"(window.data = { "a": 1, "b": [2, 3] };)";
    std::optional<boost::json::value> v = parse_json_var("data", js);
    REQUIRE(v.has_value());
    REQUIRE(v->is_object());
    CHECK(v->as_object().at("a").as_int64() == 1);
    CHECK(v->as_object().at("b").as_array().size() == 2);
}

TEST_CASE("parse_json_var parses an array literal") {
    std::string js = R"(var nums = [10, 20, 30];)";
    std::optional<boost::json::value> v = parse_json_var("nums", js);
    REQUIRE(v.has_value());
    REQUIRE(v->is_array());
    CHECK(v->as_array().size() == 3);
    CHECK(v->as_array()[1].as_int64() == 20);
}

TEST_CASE("parse_json_var tolerates comments and trailing commas") {
    std::string js = R"(var cfg = { "a": 1, /* note */ "b": 2, };)";
    std::optional<boost::json::value> v = parse_json_var("cfg", js);
    REQUIRE(v.has_value());
    CHECK(v->as_object().at("b").as_int64() == 2);
}

TEST_CASE("parse_json_var owns its result, so temporary text is fine") {
    // string_view param + owning return: no dangling, no deleted overload.
    std::optional<boost::json::value> v = parse_json_var("x", std::string_view(R"(var x = [1, 2];)"));
    REQUIRE(v.has_value());
    CHECK(v->as_array().size() == 2);
}

TEST_CASE("parse_json_var normalizes single-quoted JS strings") {
    std::string js = "var imgs = ['a', 'b'];";
    std::optional<boost::json::value> v = parse_json_var("imgs", js);
    REQUIRE(v.has_value());
    REQUIRE(v->is_array());
    REQUIRE(v->as_array().size() == 2);
    CHECK(std::string_view(v->as_array()[0].as_string()) == "a");
}

TEST_CASE("parse_json_var single-quote normalization handles embedded and escaped quotes") {
    std::string a = R"(var a = ['say "hi"'];)";
    std::optional<boost::json::value> va = parse_json_var("a", a);
    REQUIRE(va.has_value());
    CHECK(std::string_view(va->as_array()[0].as_string()) == "say \"hi\"");

    std::string b = R"(var b = ['it\'s'];)";
    std::optional<boost::json::value> vb = parse_json_var("b", b);
    REQUIRE(vb.has_value());
    CHECK(std::string_view(vb->as_array()[0].as_string()) == "it's");

    // An apostrophe inside a double-quoted string is left untouched.
    std::string c = R"(var c = { "k": "it's fine" };)";
    std::optional<boost::json::value> vc = parse_json_var("c", c);
    REQUIRE(vc.has_value());
    CHECK(std::string_view(vc->as_object().at("k").as_string()) == "it's fine");
}

TEST_CASE("parse_json_var returns nullopt when the variable is absent") {
    std::string js = "var imgs = ['a', 'b'];";
    CHECK_FALSE(parse_json_var("missing", js).has_value());
}

TEST_CASE("find_json_var and parse_json_var handle a quoted-key-inside-object page shape") {
    // The array is a quoted key inside `var data = {...}`, with two decoys
    // that must be skipped: `var fullimg = data.fullimg` and `fullimg[...]`.
    std::string js =
        "var data = { \"title\": \"x\", "
        "\"fullimg\": ['https://cdn/1.jpg', 'https://cdn/2.jpg', 'https://cdn/3.jpg'] };\n"
        "var fullimg = data.fullimg;\n"
        "function f(x) { return fullimg[x - 1]; }";

    std::string_view slice = find_json_var("fullimg", js);
    CHECK(slice.substr(0, 1) == "[");
    CHECK(slice.find("3.jpg") != std::string_view::npos);

    std::optional<boost::json::value> v = parse_json_var("fullimg", js);
    REQUIRE(v.has_value());
    REQUIRE(v->is_array());
    REQUIRE(v->as_array().size() == 3);
    CHECK(std::string_view(v->as_array()[0].as_string()) == "https://cdn/1.jpg");
    CHECK(std::string_view(v->as_array()[2].as_string()) == "https://cdn/3.jpg");
}

TEST_CASE("find_json_var skips a line comment inside the literal") {
    std::string js = "var v = { \"a\": 1, // note with a } and a ]\n \"b\": 2 };";
    CHECK(find_json_var("v", js) == "{ \"a\": 1, // note with a } and a ]\n \"b\": 2 }");
}

TEST_CASE("find_json_var returns empty on an unterminated string in the literal") {
    std::string js = "var v = { \"a\": \"no closing quote";
    CHECK(find_json_var("v", js).empty());
}

TEST_CASE("find_json_var returns empty on an unterminated block comment") {
    std::string js = "var v = { /* never closed";
    CHECK(find_json_var("v", js).empty());
}

TEST_CASE("find_json_var returns empty on a mismatched bracket") {
    std::string js = "var v = {]};";
    CHECK(find_json_var("v", js).empty());
}

TEST_CASE("find_json_var returns empty for an empty variable name") {
    std::string js = "var v = [1, 2];";
    CHECK(find_json_var("", js).empty());
}

TEST_CASE("find_json_var ignores a name that is not an assignment") {
    std::string js = "foo(); bar = foo + 1;";
    CHECK(find_json_var("foo", js).empty());
}

TEST_CASE("find_json_var ignores == and => that are not assignments") {
    std::string equality = "if (foo == bar) { baz(); }";
    std::string arrow    = "const g = foo => bar;";
    CHECK(find_json_var("foo", equality).empty());
    CHECK(find_json_var("foo", arrow).empty());
}

TEST_CASE("parse_json_var normalizes an escaped quote inside a double-quoted string") {
    std::string js = R"(var v = { "s": "say \"hi\"" };)";
    std::optional<boost::json::value> v = parse_json_var("v", js);
    REQUIRE(v.has_value());
    CHECK(std::string_view(v->as_object().at("s").as_string()) == "say \"hi\"");
}

TEST_CASE("parse_json_var keeps a non-quote escape inside a single-quoted string") {
    std::string js = R"(var v = ['a\nb'];)";
    std::optional<boost::json::value> v = parse_json_var("v", js);
    REQUIRE(v.has_value());
    CHECK(std::string_view(v->as_array()[0].as_string()) == "a\nb");
}

TEST_CASE("parse_json_var passes a line comment through normalization") {
    std::string js = "var v = { \"a\": 1 // trailing note\n };";
    std::optional<boost::json::value> v = parse_json_var("v", js);
    REQUIRE(v.has_value());
    CHECK(v->as_object().at("a").as_int64() == 1);
}

TEST_CASE("parse_json_var returns nullopt when the literal is not valid JSON") {
    // Balanced braces, so the scan yields a slice, but an unquoted key keeps
    // Boost.JSON from parsing it even with comments/trailing commas allowed.
    std::string js = "var v = { a: 1 };";
	CHECK_FALSE(parse_json_var("v", js).has_value());
}

TEST_CASE("find_json_call_argument selects a JSON literal by argument position") {
	std::string js = R"(Player.init(options, build({ "unused": true }), /* config */ { 'quality': 720, }, []);)";
	CHECK(find_json_call_argument("Player.init", 0, js).empty());
	CHECK(find_json_call_argument("Player.init", 1, js).empty());
	CHECK(find_json_call_argument("Player.init", 2, js) == R"({ 'quality': 720, })");
	CHECK(find_json_call_argument("Player.init", 3, js) == "[]");
}

TEST_CASE("parse_json_call_argument tolerates nested expressions and JS syntax") {
	std::string js = R"(Player.init(call(")", /* , */ [1, 2]), { 'name': 'it\'s fine', });)";
	auto value = parse_json_call_argument("Player.init", 1, js);
	REQUIRE(value.has_value());
	REQUIRE(value->is_object());
	CHECK(std::string_view(value->as_object().at("name").as_string()) == "it's fine");
}
