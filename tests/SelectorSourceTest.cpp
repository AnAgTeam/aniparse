/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/html/HTMLDocument.hpp>
#include <aniparse/html/SelectorSource.hpp>

#include <utility>
#include <vector>

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

TEST_CASE("a scoped view sees only its own parser's overrides") {
	// Schema v2: short keys nested under the parser identifier, so one parser's
	// hotfix cannot name another parser's selector.
	SelectorSource source({}, {
	    { "parser_a", { { "info.title", "h1.override" } } },
	    { "parser_b", { { "info.title", "h1.other" } } },
	});
	CHECK(source.has_scope("parser_a"));
	CHECK(source.has_scope("parser_b"));
	CHECK_FALSE(source.has_scope("parser_c"));
	// A scoped-only source is not "empty" even with no flat entries.
	CHECK_FALSE(source.empty_source());

	auto view_a = source.scoped_to("parser_a");
	auto view_b = source.scoped_to("parser_b");
	auto view_c = source.scoped_to("parser_c");
	CHECK(view_a->get("info.title", ".default") == "h1.override"sv);
	CHECK(view_b->get("info.title", ".default") == "h1.other"sv);
	// An id the catalog does not cover falls back to the built-in literal.
	CHECK(view_c->get("info.title", ".default") == ".default"sv);
}

TEST_CASE("a scoped view keeps the flat table and loses to it on nothing") {
	// Flat (v1, fully-spelled) keys stay visible from every scope; a scoped entry
	// wins only on an exact same key, which the two naming shapes never produce.
	SelectorSource source({ { "example.info.title", "h1.flat" } }, {
	    { "example", { { "info.title", "h1.scoped" } } },
	});

	auto view = source.scoped_to("example");
	CHECK(view->get("example.info.title", ".default") == "h1.flat"sv);
	CHECK(view->get("info.title", ".default") == "h1.scoped"sv);
}

TEST_CASE("a scoped view degrades a broken override to the default") {
	SelectorSource source({}, {
	    { "parser_a", { { "info.title", "a[" } } },
	});
	auto view = source.scoped_to("parser_a");
	CHECK(view->get("info.title", ".default") == "a["sv);

	SelectorCompiler compiler;
	CHECK_NOTHROW(view->compile(compiler, "info.title", ".default"));
}

namespace {
/// A probe set that records which override text it was built with.
struct ProbeSet {
	std::string css;
	static ProbeSet create(const SelectorSource& source) {
		return { std::string(source.get("info.title", ".default")) };
	}
};
} // namespace

TEST_CASE("the holder caches compiled sets per (parser id, set type)") {
	SelectorSourceHolder holder(std::make_shared<const SelectorSource>(
	    SelectorSource::Overrides{}, SelectorSource::ScopedOverrides{
	        { "parser_a", { { "info.title", "h1.a" } } },
	        { "parser_b", { { "info.title", "h1.b" } } },
	    }));

	// Two parsers sharing one set type get one copy EACH, with their own override.
	auto set_a = holder.set_for<ProbeSet>("parser_a");
	auto set_b = holder.set_for<ProbeSet>("parser_b");
	CHECK(set_a->css == "h1.a");
	CHECK(set_b->css == "h1.b");
	CHECK(set_a != set_b);

	// A repeated request hits the cache: same instance, no rebuild.
	CHECK(holder.set_for<ProbeSet>("parser_a") == set_a);
	// An uncovered id gets the built-in default.
	CHECK(holder.set_for<ProbeSet>("parser_c")->css == ".default");
}

TEST_CASE("holder set() invalidates cached views and sets") {
	SelectorSourceHolder holder(std::make_shared<const SelectorSource>(
	    SelectorSource::Overrides{}, SelectorSource::ScopedOverrides{
	        { "parser_a", { { "info.title", "h1.before" } } },
	    }));
	auto before = holder.set_for<ProbeSet>("parser_a");
	auto view_before = holder.view_for("parser_a");
	CHECK(before->css == "h1.before");

	// A catalog apply swaps the source: the next build must see the new overrides.
	holder.set(std::make_shared<const SelectorSource>(
	    SelectorSource::Overrides{}, SelectorSource::ScopedOverrides{
	        { "parser_a", { { "info.title", "h1.after" } } },
	    }));
	auto after = holder.set_for<ProbeSet>("parser_a");
	CHECK(after->css == "h1.after");
	CHECK(after != before);
	CHECK(holder.view_for("parser_a") != view_before);

	// Swapping to an empty source reads as every built-in literal.
	holder.set(nullptr);
	CHECK(holder.set_for<ProbeSet>("parser_a")->css == ".default");
}

TEST_CASE("an id with no scoped overrides gets the shared source itself") {
	auto source = std::make_shared<const SelectorSource>(
	    SelectorSource::Overrides{ { "info.title", "h1.flat" } });
	SelectorSourceHolder holder(source);
	// No view materialization needed: the flat-only source IS the view.
	CHECK(holder.view_for("parser_a") == source);
	CHECK(holder.view_for("") == source);
	CHECK(holder.set_for<ProbeSet>("parser_a")->css == "h1.flat");
}

namespace {
/// A recording subclass (the catalog dumper's shape): logs every (key, fallback)
/// pair a set declares, delegating the decision to the base implementation.
class RecordingSelectorSource : public SelectorSource {
public:
	using SelectorSource::SelectorSource;

	[[nodiscard]] CompiledSelector compile(SelectorCompiler& compiler,
	                                       std::string_view key,
	                                       std::string_view fallback) const override {
		recorded.emplace_back(key, fallback);
		return SelectorSource::compile(compiler, key, fallback);
	}

	mutable std::vector<std::pair<std::string, std::string>> recorded;
};
} // namespace

TEST_CASE("a recording subclass sees exactly the (key, fallback) pairs a set declares") {
	RecordingSelectorSource source;
	SelectorCompiler compiler;
	auto card = source.compile(compiler, "card.card", ".tiles .tile");
	auto title = source.compile(compiler, "info.title", "h1.main");

	CHECK(source.recorded == std::vector<std::pair<std::string, std::string>>{
	    { "card.card", ".tiles .tile" },
	    { "info.title", "h1.main" },
	});

	// Delegation: the compiled result matches identically to the base class's.
	HTMLDocument document = parse_html(
	    R"(<!DOCTYPE html><html><body><div class="tiles"><div class="tile">x</div></div><h1 class="main">t</h1></body></html>)");
	const SelectorSource& base = SelectorSource::empty();
	SelectorCompiler base_compiler;
	CHECK(document.query_all(card).size() ==
	      document.query_all(base.compile(base_compiler, "card.card", ".tiles .tile")).size());
	CHECK(document.query_all(title).size() ==
	      document.query_all(base.compile(base_compiler, "info.title", "h1.main")).size());
}

TEST_CASE("a recording subclass preserves the override semantics of the base") {
	SelectorSource::Overrides overrides{ { "card.card", ".hotfix" } };
	RecordingSelectorSource recording(overrides);
	SelectorSource base(overrides);

	SelectorCompiler recording_compiler;
	SelectorCompiler base_compiler;
	auto via_recording = recording.compile(recording_compiler, "card.card", ".tiles .tile");
	auto via_base = base.compile(base_compiler, "card.card", ".tiles .tile");

	// The (key, fallback) pair is still recorded verbatim even when the override wins.
	CHECK(recording.recorded == std::vector<std::pair<std::string, std::string>>{
	    { "card.card", ".tiles .tile" },
	});

	HTMLDocument document = parse_html(
	    R"(<!DOCTYPE html><html><body><div class="hotfix">h</div><div class="tiles"><div class="tile">x</div></div></body></html>)");
	CHECK(document.query_all(via_recording).size() == document.query_all(via_base).size());
	CHECK(document.query_all(via_recording).size() == 1); // both picked the override
}
