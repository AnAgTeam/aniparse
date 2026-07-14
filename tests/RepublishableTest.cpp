/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"

#include <aniparse/cache/EnsurePublished.hpp>
#include <aniparse/cache/Republishable.hpp>
#include <aniparse/cache/ResourceCache.hpp>

#include <memory>
#include <string>

using namespace aniparse;

namespace {

std::shared_ptr<const std::string> str(std::string value) {
	return std::make_shared<const std::string>(std::move(value));
}

// A fetch that always succeeds with @p value, counting its invocations so a test
// can prove the cache-first path skips it on a warm cell.
auto ok_fetch(int& calls, std::string value) {
	return [&calls, value]() -> NetworkRequestTask<std::shared_ptr<const std::string>> {
		++calls;
		co_return str(value);
	};
}

// A fetch that always fails, likewise counting invocations.
auto failing_fetch(int& calls) {
	return [&calls]() -> NetworkRequestTask<std::shared_ptr<const std::string>> {
		++calls;
		co_return make_response_error(RequestErrorCode::NetworkError, "boom");
	};
}

} // namespace

TEST_CASE("Republishable without a seed is cold and null until published") {
	Republishable<std::string> cell;

	CHECK_FALSE(cell.is_fetched());
	CHECK(cell.snapshot() == nullptr);

	cell.publish(str("warm"));

	CHECK(cell.is_fetched());
	REQUIRE(cell.snapshot() != nullptr);
	CHECK(*cell.snapshot() == "warm");
}

TEST_CASE("Republishable serves the seed until the first publish") {
	Republishable<std::string> cell{str("seed")};

	CHECK_FALSE(cell.is_fetched()); // seed is not a fetched snapshot
	REQUIRE(cell.snapshot() != nullptr);
	CHECK(*cell.snapshot() == "seed");

	cell.publish(str("fresh"));

	CHECK(cell.is_fetched());
	CHECK(*cell.snapshot() == "fresh");
}

TEST_CASE("Republishable republish swaps the snapshot, old handles stay valid") {
	Republishable<std::string> cell;
	cell.publish(str("first"));

	std::shared_ptr<const std::string> held = cell.snapshot(); // pin the first snapshot
	cell.publish(str("second"));

	CHECK(*cell.snapshot() == "second"); // readers now see the new one
	CHECK(*held == "first");             // the pinned handle survives the swap
}

TEST_CASE("Republishable lives in ResourceCache as a shared, publishable value") {
	ResourceCache cache;

	auto a = cache.get<Republishable<std::string>>();
	auto b = cache.get<Republishable<std::string>>();
	CHECK(a.get() == b.get()); // one instance, cached by type

	a->publish(str("via-a"));
	// The publish is visible through any handle to the same cached cell.
	REQUIRE(b->snapshot() != nullptr);
	CHECK(*b->snapshot() == "via-a");
}

TEST_CASE("Republishable seeded through the ResourceCache factory overload") {
	ResourceCache cache;

	auto seed = str("default-host");
	auto cell = cache.get<Republishable<std::string>>(
	    [seed] { return Republishable<std::string>{seed}; });

	CHECK_FALSE(cell->is_fetched());
	CHECK(*cell->snapshot() == "default-host");
}

CORO_TEST_CASE("ensure_published fetches once on a cold cell, then serves warm") {
	ResourceCache cache;
	auto cell = cache.get<Republishable<std::string>>();

	int calls = 0;

	auto first = co_await ensure_published(cell, ok_fetch(calls, "value"));
	REQUIRE(first.has_value());
	REQUIRE(*first != nullptr);
	CHECK(**first == "value");
	CHECK(calls == 1);
	CHECK(cell->is_fetched());

	// Warm cell: the fetch must not run again.
	auto second = co_await ensure_published(cell, ok_fetch(calls, "ignored"));
	REQUIRE(second.has_value());
	CHECK(**second == "value");
	CHECK(calls == 1);
}

CORO_TEST_CASE("ensure_published propagates the fetch error and stays cold") {
	ResourceCache cache;
	auto cell = cache.get<Republishable<std::string>>();

	int calls = 0;
	auto result = co_await ensure_published(cell, failing_fetch(calls));

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().code == RequestErrorCode::NetworkError);
	CHECK(calls == 1);
	CHECK_FALSE(cell->is_fetched()); // stays cold so a later call retries
}

CORO_TEST_CASE("ensure_published_or_seed serves the seed on failure, no error") {
	ResourceCache cache;
	auto seed = str("seed-host");
	auto cell = cache.get<Republishable<std::string>>(
	    [seed] { return Republishable<std::string>{seed}; });

	int calls = 0;
	auto result = co_await ensure_published_or_seed(cell, failing_fetch(calls));

	REQUIRE(result.has_value());  // no error surfaced
	REQUIRE(*result != nullptr);
	CHECK(**result == "seed-host"); // the seed, served as fallback
	CHECK(calls == 1);
	CHECK_FALSE(cell->is_fetched()); // still cold -> retries next time
}

CORO_TEST_CASE("ensure_published_or_seed publishes on success") {
	ResourceCache cache;
	auto cell = cache.get<Republishable<std::string>>();

	int calls = 0;
	auto result = co_await ensure_published_or_seed(cell, ok_fetch(calls, "live"));

	REQUIRE(result.has_value());
	CHECK(**result == "live");
	CHECK(calls == 1);
	CHECK(cell->is_fetched());
}
