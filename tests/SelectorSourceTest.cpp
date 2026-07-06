/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/html/SelectorSource.hpp>

using namespace aniparse::html;
using namespace std::string_view_literals;

TEST_CASE("SelectorSource empty falls back to the built-in literal") {
	const SelectorSource& source = SelectorSource::empty();
	CHECK(source.empty_source());
	CHECK(source.get("any.key", ".fallback") == ".fallback"sv);

	SelectorCompiler compiler;
	CHECK(static_cast<bool>(source.compile(compiler, "any.key", ".fallback")));
}

TEST_CASE("SelectorSource prefers a present, valid override") {
	SelectorSource source({ { "info.description", "#override" } });
	CHECK_FALSE(source.empty_source());
	CHECK(source.get("info.description", "#default") == "#override"sv);

	SelectorCompiler compiler;
	CHECK(static_cast<bool>(source.compile(compiler, "info.description", "#default")));
}

TEST_CASE("SelectorSource falls back when the key has no override") {
	SelectorSource source({ { "some.other.key", "#present" } });
	CHECK(source.get("info.description", "#default") == "#default"sv);

	SelectorCompiler compiler;
	CHECK(static_cast<bool>(source.compile(compiler, "info.description", "#default")));
}

TEST_CASE("SelectorSource compile() degrades a broken override to the default") {
	// A malformed override off the net must not brick set construction: get() still
	// exposes the raw string, but compile() drops it and compiles the built-in
	// default instead — the path the tests already cover.
	SelectorSource source({ { "info.description", "a[" } });
	CHECK(source.get("info.description", "#default") == "a["sv);

	SelectorCompiler compiler;
	// Would throw if compile() tried the broken override with the strict compile();
	// instead it try_compiles the override, fails, and falls back to "#default".
	CHECK_NOTHROW(source.compile(compiler, "info.description", "#default"));
	CHECK(static_cast<bool>(source.compile(compiler, "info.description", "#default")));
}
