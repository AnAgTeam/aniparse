/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/utility/RegexSource.hpp>

#include <stdexcept>
#include <string_view>

using namespace aniparse;
using namespace std::string_view_literals;

TEST_CASE("RegexSource empty falls back to the built-in literal") {
	const RegexSource& source = RegexSource::empty();
	CHECK(source.empty_source());
	CHECK(source.get("any.key", R"(/v/[^/]+\.mp4)") == R"(/v/[^/]+\.mp4)"sv);

	regex compiled = source.compile("any.key", R"(/v/[^/]+\.mp4)");
	svmatch match;
	CHECK(regex_search("/v/abc.mp4"sv, match, compiled));
}

TEST_CASE("RegexSource prefers a present, valid override") {
	RegexSource source(RegexSource::Overrides{ { "info.id", R"(/x/(\d+))" } });
	CHECK_FALSE(source.empty_source());
	CHECK(source.get("info.id", "/d/def") == R"(/x/(\d+))"sv);

	regex compiled = source.compile("info.id", "/d/def");
	svmatch match;
	REQUIRE(regex_search("/x/42"sv, match, compiled));
	CHECK(std::string_view(match[1].first, match[1].second) == "42"sv);
}

TEST_CASE("RegexSource falls back when the key has no override") {
	RegexSource source(RegexSource::Overrides{ { "other.key", "/x/" } });
	CHECK(source.get("info.id", "/d/def") == "/d/def"sv);
	CHECK_NOTHROW(source.compile("info.id", "/d/def"));
}

TEST_CASE("RegexSource compile() degrades a broken override to the default") {
	// A malformed override off the net must not brick set construction: get()
	// still exposes the raw string, but compile() drops it.
	RegexSource source(RegexSource::Overrides{ { "info.id", "a[" } });
	CHECK(source.get("info.id", "/d/def") == "a["sv);
	CHECK_NOTHROW(source.compile("info.id", "/d/def"));
}

TEST_CASE("RegexSource rejects an override missing a required named group") {
	// The consumer reads match["id"] back; an override that compiles but does not
	// declare (?<id>...) would silently feed it an empty capture, so compile()
	// refuses it and falls back to the built-in.
	RegexSource source(RegexSource::Overrides{ { "info.id", R"(/x/(\d+))" } });
	const std::string_view groups[] = { "id" };
	regex compiled = source.compile("info.id", R"(/d/(?<id>\d+))", groups);
	svmatch match;
	// The built-in won: it matches /d/, not the override's /x/.
	REQUIRE(regex_search("/d/42"sv, match, compiled));
	CHECK(std::string_view(match["id"].first, match["id"].second) == "42"sv);
}

TEST_CASE("RegexSource accepts an override declaring the required group") {
	RegexSource source(RegexSource::Overrides{ { "info.id", R"(/x/(?<id>\d+))" } });
	const std::string_view groups[] = { "id" };
	regex compiled = source.compile("info.id", R"(/d/(?<id>\d+))", groups);
	svmatch match;
	REQUIRE(regex_search("/x/7"sv, match, compiled));
	CHECK(std::string_view(match["id"].first, match["id"].second) == "7"sv);
}

TEST_CASE("RegexSource throws when the built-in misses its own contracted group") {
	const RegexSource& source = RegexSource::empty();
	const std::string_view groups[] = { "id" };
	// A default that does not declare the group its consumer reads is a
	// programming error, not a runtime condition.
	CHECK_THROWS_AS(source.compile("info.id", R"(/d/(\d+))", groups), std::logic_error);
}

TEST_CASE("reading an undeclared named group yields an empty match") {
	// Pin the boost behavior the group-contract checks rely on: indexing a match
	// by a name the pattern never declared must be defined behavior (an empty,
	// unmatched sub_match), not UB.
	regex expression(R"(/d/(\d+))");
	svmatch match;
	REQUIRE(regex_search("/d/42"sv, match, expression));
	CHECK_FALSE(match["never_declared"].matched);
	CHECK(match["never_declared"].first == match["never_declared"].second);
}

TEST_CASE("a scoped view sees only its own owner's overrides") {
	RegexSource source({
	    { "owner_a", { { "info.id", R"(/a/(\d+))" } } },
	    { "owner_b", { { "info.id", R"(/b/(\d+))" } } },
	});
	CHECK(source.has_scope("owner_a"));
	CHECK(source.has_scope("owner_b"));
	CHECK_FALSE(source.has_scope("owner_c"));
	CHECK_FALSE(source.empty_source());

	auto view_a = source.scoped_to("owner_a");
	auto view_c = source.scoped_to("owner_c");
	CHECK(view_a->get("info.id", "/def") == R"(/a/(\d+))"sv);
	// An id the catalog does not cover falls back to the built-in literal.
	CHECK(view_c->get("info.id", "/def") == "/def"sv);
	CHECK(view_c->empty_source());
}

namespace {
/// A probe set that records which override text it was built with.
struct ProbeSet {
	std::string pattern;
	static ProbeSet create(const RegexSource& source) {
		return { std::string(source.get("info.id", "/default")) };
	}
};
} // namespace

TEST_CASE("RegexSourceHolder caches compiled sets per (owner id, set type)") {
	RegexSourceHolder holder(std::make_shared<const RegexSource>(RegexSource::ScopedOverrides{
	    { "owner_a", { { "info.id", "/a" } } },
	    { "owner_b", { { "info.id", "/b" } } },
	}));

	// Two owners sharing one set type get one copy EACH, with their own override.
	auto set_a = holder.set_for<ProbeSet>("owner_a");
	auto set_b = holder.set_for<ProbeSet>("owner_b");
	CHECK(set_a->pattern == "/a");
	CHECK(set_b->pattern == "/b");
	CHECK(set_a != set_b);

	// A repeated request hits the cache: same instance, no rebuild.
	CHECK(holder.set_for<ProbeSet>("owner_a") == set_a);
	// An uncovered id gets the built-in default.
	CHECK(holder.set_for<ProbeSet>("owner_c")->pattern == "/default");
}

TEST_CASE("RegexSourceHolder set() invalidates cached views and sets") {
	RegexSourceHolder holder(std::make_shared<const RegexSource>(RegexSource::ScopedOverrides{
	    { "owner_a", { { "info.id", "/before" } } },
	}));
	auto before = holder.set_for<ProbeSet>("owner_a");
	auto view_before = holder.view_for("owner_a");
	CHECK(before->pattern == "/before");

	// A catalog apply swaps the source: the next build must see the new overrides.
	holder.set(std::make_shared<const RegexSource>(RegexSource::ScopedOverrides{
	    { "owner_a", { { "info.id", "/after" } } },
	}));
	auto after = holder.set_for<ProbeSet>("owner_a");
	CHECK(after->pattern == "/after");
	CHECK(after != before);
	CHECK(holder.view_for("owner_a") != view_before);

	// Swapping to an empty source reads as every built-in literal.
	holder.set(nullptr);
	CHECK(holder.set_for<ProbeSet>("owner_a")->pattern == "/default");
}

TEST_CASE("RegexSourceHolder gives an unscoped id the shared source itself") {
	auto source = std::make_shared<const RegexSource>(RegexSource::ScopedOverrides{
	    { "owner_a", { { "info.id", "/a" } } },
	});
	RegexSourceHolder holder(source);
	// No view materialization needed: the unscoped source IS the view.
	CHECK(holder.view_for("owner_b") == source);
	CHECK(holder.view_for("") == source);
	CHECK(holder.set_for<ProbeSet>("owner_b")->pattern == "/default");
}
