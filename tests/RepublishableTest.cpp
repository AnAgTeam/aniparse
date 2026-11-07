/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"

#include <aniparse/cache/EnsurePublished.hpp>
#include <aniparse/cache/Republishable.hpp>
#include <aniparse/cache/ResourceCache.hpp>

#include <coro/event.hpp>

#include <atomic>
#include <barrier>
#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include <string>

using namespace aniparse;

namespace {

std::shared_ptr<const std::string> str(std::string value) {
	return std::make_shared<const std::string>(std::move(value));
}

// A fetch that always succeeds with @p value, counting its invocations so a test
// can prove the cache-first path skips it on a warm cell.
auto ok_fetch(int& calls, std::string value) {
	return [&calls, value]() -> RequestTask<std::shared_ptr<const std::string>> {
		++calls;
		co_return str(value);
	};
}

// A fetch that always fails, likewise counting invocations.
auto failing_fetch(int& calls) {
	return [&calls]() -> RequestTask<std::shared_ptr<const std::string>> {
		++calls;
		co_return make_request_error(RequestErrorCode::NetworkError, "boom");
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

namespace {
using Snapshot = std::shared_ptr<const std::string>;
using LoadTask = RequestTask<Snapshot>;

// The gate keeps all callers overlapping without sleeps or network requests.
auto gated_fetch(coro::event& gate, int& calls, Result<Snapshot> outcome) {
    return [&gate, &calls, outcome = std::move(outcome)]() -> LoadTask {
        ++calls;
        co_await gate;
        co_return outcome;
    };
}

Result<Snapshot> completed(LoadTask& task) {
    REQUIRE(task.done());
    return task.promise().result();
}
}

TEST_CASE("Concurrent cache callers share one successful fetch and snapshot") {
    auto cell = std::make_shared<const Republishable<std::string>>();
    coro::event gate;
    int calls = 0;
    auto expected = str("shared");
    auto fetch = gated_fetch(gate, calls, expected);
    auto first = ensure_published(cell, fetch);
    auto second = ensure_published(cell, fetch);
    auto third = ensure_published_or_seed(cell, fetch);
    first.resume();
    second.resume();
    third.resume();
    CHECK(calls == 1);
    CHECK_FALSE(first.done());
    CHECK_FALSE(second.done());
    CHECK_FALSE(third.done());
    gate.set();
    CHECK(completed(first).value() == expected);
    CHECK(completed(second).value() == expected);
    CHECK(completed(third).value() == expected);
    CHECK(cell->snapshot() == expected);
}

TEST_CASE("An unsuccessful flight shares its error and permits a later retry") {
    auto cell = std::make_shared<const Republishable<std::string>>(str("seed"));
    coro::event gate;
    int calls = 0;
    auto fetch = gated_fetch(gate, calls,
        make_request_error(RequestErrorCode::RateLimited, "try later", 429, "details"));
    auto first = ensure_published(cell, fetch);
    auto second = ensure_published(cell, fetch);
    auto fallback = ensure_published_or_seed(cell, fetch);
    first.resume();
    second.resume();
    fallback.resume();
    gate.set();
    auto first_result = completed(first);
    auto second_result = completed(second);
    REQUIRE_FALSE(first_result);
    REQUIRE_FALSE(second_result);
    CHECK(second_result.error().code == RequestErrorCode::RateLimited);
    CHECK(second_result.error().http_status == 429);
    CHECK(second_result.error().body == "details");
    CHECK(completed(fallback).value() == cell->snapshot());
    CHECK(calls == 1);
    CHECK_FALSE(cell->is_fetched());
    auto retry = ensure_published(cell, ok_fetch(calls, "recovered"));
    retry.resume();
    CHECK(*completed(retry).value() == "recovered");
    CHECK(calls == 2);
}

TEST_CASE("Fetch exceptions wake every waiter and leave the cache retryable") {
    auto cell = std::make_shared<const Republishable<std::string>>();
    coro::event gate;
    int calls = 0;
    auto fetch = [&]() -> LoadTask {
        ++calls;
        co_await gate;
        throw std::runtime_error("fetch exception");
        co_return nullptr;
    };
    auto first = ensure_published(cell, fetch);
    auto second = ensure_published(cell, fetch);
    first.resume();
    second.resume();
    gate.set();
    REQUIRE(first.done());
    REQUIRE(second.done());
    CHECK_THROWS_WITH(first.promise().result(), "fetch exception");
    CHECK_THROWS_WITH(second.promise().result(), "fetch exception");
    CHECK(calls == 1);
    auto retry = ensure_published(cell, ok_fetch(calls, "retry"));
    retry.resume();
    CHECK(*completed(retry).value() == "retry");
}

TEST_CASE("Destroying a suspended waiter unregisters it without cancelling the fetch") {
    auto cell = std::make_shared<const Republishable<std::string>>();
    coro::event gate;
    int calls = 0;
    auto fetch = gated_fetch(gate, calls, str("survived"));
    auto first = ensure_published(cell, fetch);
    first.resume();
    {
        auto abandoned = ensure_published(cell, fetch);
        abandoned.resume();
        CHECK_FALSE(abandoned.done());
    }
    auto survivor = ensure_published(cell, fetch);
    survivor.resume();
    gate.set();
    CHECK(*completed(first).value() == "survived");
    CHECK(*completed(survivor).value() == "survived");
    CHECK(calls == 1);
}

TEST_CASE("Destroying the producer wakes waiters and releases the flight") {
    auto cell = std::make_shared<const Republishable<std::string>>();
    int calls = 0;
    auto suspended = [&]() -> LoadTask {
        ++calls;
        co_await std::suspend_always{};
        co_return str("unreachable");
    };
    auto first = std::make_unique<LoadTask>(ensure_published(cell, suspended));
    auto second = ensure_published(cell, suspended);
    first->resume();
    second.resume();
    first.reset();
    auto result = completed(second);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == RequestErrorCode::Cancelled);
    CHECK_FALSE(cell->is_fetched());
    auto retry = ensure_published(cell, ok_fetch(calls, "retry"));
    retry.resume();
    CHECK(*completed(retry).value() == "retry");
    CHECK(calls == 2);
}

TEST_CASE("A cancelled waiter does not cancel the producer or other waiters") {
    auto cell = std::make_shared<const Republishable<std::string>>();
    coro::event gate;
    int calls = 0;
    auto fetch = gated_fetch(gate, calls, str("loaded"));
    auto first = ensure_published(cell, fetch);
    auto second = ensure_published(cell, fetch);
    first.resume();
    second.resume();
    second.get_stop_source().request_stop();
    gate.set();
    CHECK(completed(first).has_value());
    auto cancelled = completed(second);
    REQUIRE_FALSE(cancelled);
    CHECK(cancelled.error().code == RequestErrorCode::Cancelled);
    CHECK(cell->is_fetched());
    CHECK(calls == 1);
}

TEST_CASE("Producer cancellation is shared without publishing its result") {
    auto cell = std::make_shared<const Republishable<std::string>>();
    coro::event gate;
    int calls = 0;
    auto fetch = gated_fetch(gate, calls, str("cancelled"));
    auto first = ensure_published(cell, fetch);
    auto second = ensure_published(cell, fetch);
    first.resume();
    second.resume();
    first.get_stop_source().request_stop();
    gate.set();
    auto a = completed(first);
    auto b = completed(second);
    REQUIRE_FALSE(a);
    REQUIRE_FALSE(b);
    CHECK(a.error().code == RequestErrorCode::Cancelled);
    CHECK(b.error().code == RequestErrorCode::Cancelled);
    CHECK_FALSE(cell->is_fetched());
}

TEST_CASE("Concurrent threads elect only one cache producer") {
    constexpr int callers = 16;
    auto cell = std::make_shared<const Republishable<std::string>>();
    std::atomic<int> fetches{0};
    coro::event gate;
    std::barrier start(callers);
    auto fetch = [&]() -> LoadTask {
        fetches.fetch_add(1);
        co_await gate;
        co_return str("threaded");
    };
    std::vector<LoadTask> tasks;
    tasks.reserve(callers);
    for (int i = 0; i < callers; ++i) tasks.push_back(ensure_published(cell, fetch));
    std::vector<std::thread> threads;
    for (auto& task : tasks) {
        threads.emplace_back([&start, task_ptr = &task] {
            start.arrive_and_wait();
            task_ptr->resume();
        });
    }
    for (auto& thread : threads) thread.join();
    CHECK(fetches.load() == 1);
    gate.set();
    for (auto& task : tasks) CHECK(*completed(task).value() == "threaded");
}

TEST_CASE("An in-flight cache entry survives eviction") {
    ResourceCache cache;
    auto cell = cache.get<Republishable<std::string>>();
    std::weak_ptr<const Republishable<std::string>> retained = cell;
    coro::event gate;
    int calls = 0;
    auto fetch = gated_fetch(gate, calls, str("retained"));
    auto first = ensure_published(cell, fetch);
    auto second = ensure_published(cell, fetch);
    first.resume();
    second.resume();
    cache.clear();
    cell.reset();
    CHECK_FALSE(retained.expired());
    gate.set();
    CHECK(completed(first).value() == completed(second).value());
    CHECK(calls == 1);
}

TEST_CASE("A resumed waiter can destroy another waiter before it is notified") {
    auto cell = std::make_shared<const Republishable<std::string>>();
    coro::event gate;
    int calls = 0;
    auto fetch = gated_fetch(gate, calls, str("shared"));
    std::optional<LoadTask> removed;
    auto destroy_next = [&]() -> LoadTask {
        auto result = co_await ensure_published(cell, fetch);
        removed.reset();
        co_return result;
    };
    auto producer = ensure_published(cell, fetch);
    auto first_waiter = destroy_next();
    producer.resume();
    first_waiter.resume();
    removed.emplace(ensure_published(cell, fetch));
    removed->resume();
    gate.set();
    CHECK_FALSE(removed.has_value());
    CHECK(completed(producer).value() == completed(first_waiter).value());
}

TEST_CASE("Completion racing with waiter registration loses no wakeups") {
    constexpr int repetitions = 100;
    for (int iteration = 0; iteration < repetitions; ++iteration) {
        auto cell = std::make_shared<const Republishable<std::string>>();
        coro::event gate;
        int calls = 0;
        auto fetch = gated_fetch(gate, calls, str("ready"));
        auto producer = ensure_published(cell, fetch);
        auto waiter = ensure_published(cell, fetch);
        producer.resume();
        std::barrier start(2);
        std::thread registering([&] {
            start.arrive_and_wait();
            waiter.resume();
        });
        start.arrive_and_wait();
        gate.set();
        registering.join();
        CHECK(completed(waiter).value() == completed(producer).value());
        CHECK(calls == 1);
    }
}
