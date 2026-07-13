/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/types/Headers.hpp>

using namespace aniparse;

TEST_CASE("parse_header_block parses Name: Value lines", "[headers]") {
	Headers headers = parse_header_block(
	    "Content-Type: text/html\r\n"
	    "X-Foo: bar\r\n");

	REQUIRE(headers.size() == 2);
	REQUIRE(headers.at("Content-Type") == "text/html");
	REQUIRE(headers.at("X-Foo") == "bar");
}

TEST_CASE("parse_header_block lookup is case-insensitive", "[headers]") {
	Headers headers = parse_header_block("Content-Type: application/json\r\n");

	REQUIRE(headers.contains("content-type"));
	REQUIRE(headers.at("CONTENT-TYPE") == "application/json");
}

TEST_CASE("parse_header_block trims optional whitespace around the value", "[headers]") {
	Headers headers = parse_header_block(
	    "A:    spaced   \r\n"
	    "B:\ttabbed\t\r\n");

	REQUIRE(headers.at("A") == "spaced");
	REQUIRE(headers.at("B") == "tabbed");
}

TEST_CASE("parse_header_block keeps colons inside the value", "[headers]") {
	Headers headers = parse_header_block("Location: http://host:8080/path\r\n");

	REQUIRE(headers.at("Location") == "http://host:8080/path");
}

TEST_CASE("parse_header_block skips the status line, blanks and malformed lines", "[headers]") {
	Headers headers = parse_header_block(
	    "HTTP/1.1 200 OK\r\n"
	    "Server: test\r\n"
	    "garbage-without-a-colon\r\n"
	    "\r\n");

	REQUIRE(headers.size() == 1);
	REQUIRE(headers.at("Server") == "test");
}

TEST_CASE("parse_header_block tolerates bare LF and a missing trailing newline", "[headers]") {
	Headers headers = parse_header_block(
	    "A: 1\n"
	    "B: 2");

	REQUIRE(headers.size() == 2);
	REQUIRE(headers.at("A") == "1");
	REQUIRE(headers.at("B") == "2");
}

TEST_CASE("parse_header_block keeps duplicate names in order", "[headers]") {
	Headers headers = parse_header_block(
	    "X: first\r\n"
	    "x: second\r\n");

	REQUIRE(headers.size() == 2);
	// by-name lookup returns the first; get_all exposes every value (case-insensitive).
	REQUIRE(headers.at("X") == "first");
	REQUIRE(headers.get_all("x") == std::vector<std::string_view>{"first", "second"});
}

TEST_CASE("parse_header_block accepts an empty value", "[headers]") {
	Headers headers = parse_header_block("X-Empty:\r\n");

	REQUIRE(headers.contains("X-Empty"));
	REQUIRE(headers.at("X-Empty").empty());
}

TEST_CASE("parse_header_block round-trips to_header_lines", "[headers]") {
	Headers original;
	original.set("Accept", "*/*");
	original.set("Content-Type", "text/plain");

	std::string block;
	for (const auto& line : to_header_lines(original)) {
		block += line + "\r\n";
	}

	REQUIRE(parse_header_block(block) == original);
}
