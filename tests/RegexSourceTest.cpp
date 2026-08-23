/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/utility/RegexSource.hpp>

#include <string_view>
#include <utility>
#include <vector>

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

TEST_CASE("RegexSource rejects an override missing a group the built-in declares") {
	// The consumer reads match["id"] back; an override that compiles but does not
	// declare (?<id>...) would silently feed it an empty capture, so compile()
	// refuses it and falls back to the built-in. The contract is derived from the
	// built-in literal itself — there is no separate group list to pass.
	RegexSource source(RegexSource::Overrides{ { "info.id", R"(/x/(\d+))" } });
	regex compiled = source.compile("info.id", R"(/d/(?<id>\d+))");
	svmatch match;
	// The built-in won: it matches /d/, not the override's /x/.
	REQUIRE(regex_search("/d/42"sv, match, compiled));
	CHECK(std::string_view(match["id"].first, match["id"].second) == "42"sv);
}

TEST_CASE("RegexSource accepts an override declaring the built-in's group") {
	RegexSource source(RegexSource::Overrides{ { "info.id", R"(/x/(?<id>\d+))" } });
	regex compiled = source.compile("info.id", R"(/d/(?<id>\d+))");
	svmatch match;
	REQUIRE(regex_search("/x/7"sv, match, compiled));
	CHECK(std::string_view(match["id"].first, match["id"].second) == "7"sv);
}

TEST_CASE("RegexSource derives no contract from a group-free built-in") {
	// A whole-match pattern declares nothing, so its override is unconstrained.
	RegexSource source(RegexSource::Overrides{ { "video.path", R"(/v/[^/]+\.ts)" } });
	regex compiled = source.compile("video.path", R"(/v/[^/]+\.mp4)");
	svmatch match;
	CHECK(regex_search("/v/abc.ts"sv, match, compiled));
}

TEST_CASE("RegexSource does not misread lookbehinds, escapes, or char classes as groups") {
	// (?<=...)/(?!...) are lookarounds, \(\?< matches verbatim "(?<", and tokens
	// inside [...] are literal text — none of them declares a named group, so the
	// overrides below carry no contract and must be accepted as-is.
	RegexSource source(RegexSource::Overrides{
	    { "a", R"((?<=/x/)\d+)" },
	    { "b", R"(\(\?<name>\d+\))" },
	    { "c", R"([(?<']\d+)" },
	});
	svmatch match;
	CHECK(regex_search("/x/42"sv, match, source.compile("a", R"((?<=/d/)\d+)")));
	CHECK(regex_search("(?<name>42)"sv, match, source.compile("b", R"(\(\?<id>\d+\))")));
	CHECK(regex_search("<7"sv, match, source.compile("c", R"([(?<']\d+)")));
}

TEST_CASE("RegexSource throws when the built-in literal itself is broken") {
	const RegexSource& source = RegexSource::empty();
	// A broken default is a programming error, not a runtime condition.
	CHECK_THROWS_AS(source.compile("info.id", "a["), boost::regex_error);
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

namespace {
/// A recording subclass (the catalog dumper's shape): logs every (key, fallback)
/// pair a set declares, delegating the decision to the base implementation.
class RecordingRegexSource : public RegexSource {
public:
	using RegexSource::RegexSource;

	[[nodiscard]] regex compile(std::string_view key, std::string_view fallback) const override {
		recorded.emplace_back(key, fallback);
		return RegexSource::compile(key, fallback);
	}

	mutable std::vector<std::pair<std::string, std::string>> recorded;
};
} // namespace

TEST_CASE("a recording RegexSource subclass sees exactly the (key, fallback) pairs a set declares") {
	RecordingRegexSource source;
	regex compiled = source.compile("video_path", R"(/v/[^/]+\.mp4)");
	CHECK(source.recorded == std::vector<std::pair<std::string, std::string>>{
	    { "video_path", R"(/v/[^/]+\.mp4)" },
	});

	// Delegation: the compiled result matches identically to the base class's.
	const RegexSource& base = RegexSource::empty();
	regex base_compiled = base.compile("video_path", R"(/v/[^/]+\.mp4)");
	svmatch match;
	CHECK(regex_search("/v/abc.mp4"sv, match, compiled));
	CHECK(regex_search("/v/abc.mp4"sv, match, base_compiled));
	CHECK(regex_search("/v/abc.txt"sv, match, compiled) ==
	      regex_search("/v/abc.txt"sv, match, base_compiled));
}

TEST_CASE("a recording RegexSource subclass preserves the override semantics of the base") {
	RegexSource::Overrides overrides{ { "video_path", R"(/u/[^/]+\.mp4)" } };
	RecordingRegexSource recording(overrides);
	RegexSource base(overrides);

	regex via_recording = recording.compile("video_path", R"(/v/[^/]+\.mp4)");
	regex via_base = base.compile("video_path", R"(/v/[^/]+\.mp4)");

	// The (key, fallback) pair is still recorded verbatim even when the override wins.
	CHECK(recording.recorded == std::vector<std::pair<std::string, std::string>>{
	    { "video_path", R"(/v/[^/]+\.mp4)" },
	});

	svmatch match;
	CHECK(regex_search("/u/abc.mp4"sv, match, via_recording));
	CHECK(regex_search("/u/abc.mp4"sv, match, via_base));
	CHECK_FALSE(regex_search("/v/abc.mp4"sv, match, via_recording));
	CHECK_FALSE(regex_search("/v/abc.mp4"sv, match, via_base));
}
