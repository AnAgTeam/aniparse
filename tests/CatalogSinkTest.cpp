/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "catch_amalgamated.hpp"

#include <aniparse/utility/CatalogSink.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

using namespace aniparse;
using namespace std::string_view_literals;

namespace {
/// A probe selector set declaring two keys.
struct ProbeSelectors {
	html::CompiledSelector card;
	html::CompiledSelector title;
	static ProbeSelectors create(const html::SelectorSource& source) {
		html::SelectorCompiler compiler;
		return {
		    source.compile(compiler, "card.card", ".tiles .tile"),
		    source.compile(compiler, "info.title", "h1.main"),
		};
	}
};

/// A probe pattern set declaring one key.
struct ProbePatterns {
	regex video_path;
	static ProbePatterns create(const RegexSource& source) {
		return { source.compile("video_path", R"(/v/[^/]+\.mp4)") };
	}
};

/// A second selector set sharing one key with ProbeSelectors.
struct CollidingSelectors {
	html::CompiledSelector card;
	static CollidingSelectors create(const html::SelectorSource& source) {
		html::SelectorCompiler compiler;
		return { source.compile(compiler, "card.card", ".other .tile") };
	}
};

/// A probe set whose built-in literal does not compile.
struct BrokenSelectors {
	html::CompiledSelector bad;
	static BrokenSelectors create(const html::SelectorSource& source) {
		html::SelectorCompiler compiler;
		return { source.compile(compiler, "bad.key", "a[") };
	}
};
} // namespace

TEST_CASE("CatalogSink records a selector set's (key, literal) pairs") {
	CatalogSink sink("ExampleParser");
	sink.emplace_set<ProbeSelectors>();
	CHECK(sink.selectors() == CatalogSink::RecordedSet{
	    { "card.card", ".tiles .tile" },
	    { "info.title", "h1.main" },
	});
	CHECK(sink.patterns().empty());
	CHECK_FALSE(sink.canonical_base_url().has_value());
}

TEST_CASE("CatalogSink records a pattern set's (key, literal) pairs") {
	CatalogSink sink("ExampleExtractor");
	sink.emplace_set<ProbePatterns>();
	CHECK(sink.patterns() == CatalogSink::RecordedSet{
	    { "video_path", R"(/v/[^/]+\.mp4)" },
	});
	CHECK(sink.selectors().empty());
}

TEST_CASE("CatalogSink rejects a key colliding across the owner's sets") {
	CatalogSink sink("ExampleParser");
	sink.emplace_set<ProbeSelectors>();
	CHECK_THROWS_AS(sink.emplace_set<CollidingSelectors>(), std::logic_error);
}

TEST_CASE("CatalogSink rejects a second canonical base URL") {
	CatalogSink sink("ExampleParser");
	sink.set_canonical_base_url("https://example.org");
	CHECK(sink.canonical_base_url() == "https://example.org");
	CHECK_THROWS_AS(sink.set_canonical_base_url("https://mirror.example.org"),
	                std::logic_error);
}

TEST_CASE("CatalogSink with no declarations emits nothing") {
	CatalogSink sink("QuietParser");
	CHECK(sink.selectors().empty());
	CHECK(sink.patterns().empty());
	CHECK_FALSE(sink.canonical_base_url().has_value());
	CHECK(sink.owner_id() == "QuietParser"sv);
}

TEST_CASE("a broken built-in literal fails the sink loudly, naming owner and key") {
	CatalogSink sink("BrokenParser");
	try {
		sink.emplace_set<BrokenSelectors>();
		FAIL("expected std::logic_error");
	} catch (const std::logic_error& error) {
		const std::string message = error.what();
		CHECK(message.find("BrokenParser") != std::string::npos);
		CHECK(message.find("bad.key") != std::string::npos);
	}
}
