/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/ResourceCache.hpp>
#include <aniparse/html/CompiledSelector.hpp>

using namespace aniparse;

namespace {

// A trivial resource set: exercises genericity, caching identity and eviction
// without pulling in any compile machinery.
struct Counter {
	int value;

	static int build_count;

	static Counter create() {
		++build_count;
		return { 42 };
	}
};
int Counter::build_count = 0;

// A second, unrelated set: proves keys never collide across types.
struct Other {
	int value;
	static Other create() { return { 7 }; }
};

// A real selector set: proves the CSS path works through the generic cache.
struct PageSelectors {
	html::CompiledSelector thumbs;
	html::CompiledSelector content_script;

	static PageSelectors create() {
		html::SelectorCompiler compiler;
		return {
			compiler.compile("#thumbs"),
			compiler.compile("#content script"),
		};
	}
};

} // namespace

TEST_CASE("ResourceCache builds a set once and reuses it") {
	Counter::build_count = 0;
	ResourceCache cache;

	std::shared_ptr<const Counter> first  = cache.get<Counter>();
	std::shared_ptr<const Counter> second = cache.get<Counter>();

	REQUIRE(first->value == 42);
	CHECK(first.get() == second.get()); // same object, not rebuilt
	CHECK(Counter::build_count == 1);
}

TEST_CASE("ResourceCache keys by type, so sets never collide") {
	ResourceCache cache;

	auto counter = cache.get<Counter>();
	auto other   = cache.get<Other>();

	REQUIRE(counter->value == 42);
	REQUIRE(other->value == 7);
	CHECK(static_cast<const void*>(counter.get()) != static_cast<const void*>(other.get()));
}

TEST_CASE("ResourceCache evict drops a set, keeping live handles alive") {
	Counter::build_count = 0;
	ResourceCache cache;

	std::shared_ptr<const Counter> held = cache.get<Counter>();
	REQUIRE(Counter::build_count == 1);

	cache.evict<Counter>();
	CHECK(held->value == 42); // handle survives eviction

	std::shared_ptr<const Counter> rebuilt = cache.get<Counter>();
	CHECK(Counter::build_count == 2);    // recompiled after evict
	CHECK(rebuilt.get() != held.get());  // a fresh object
}

TEST_CASE("ResourceCache clear drops everything") {
	Counter::build_count = 0;
	ResourceCache cache;

	(void) cache.get<Counter>();
	(void) cache.get<Other>();
	cache.clear();

	(void) cache.get<Counter>();
	CHECK(Counter::build_count == 2); // rebuilt after clear
}

TEST_CASE("ResourceCache factory overload builds only on miss") {
	ResourceCache cache;

	int factory_calls = 0;
	auto make = [&] { ++factory_calls; return Counter{ 99 }; };

	std::shared_ptr<const Counter> first  = cache.get<Counter>(make);
	std::shared_ptr<const Counter> second = cache.get<Counter>(make);

	CHECK(first->value == 99);
	CHECK(first.get() == second.get());
	CHECK(factory_calls == 1); // second call hit the cache, factory not run
}

TEST_CASE("ResourceCache holds a real compiled selector set") {
	ResourceCache cache;

	std::shared_ptr<const PageSelectors> selectors = cache.get<PageSelectors>();

	REQUIRE(selectors->thumbs);
	REQUIRE(selectors->content_script);
	CHECK(cache.get<PageSelectors>().get() == selectors.get()); // cached
}
