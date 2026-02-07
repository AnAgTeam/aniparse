/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include <aniparse/utility/Coroutines.hpp>

#include <coro/task.hpp>
#include <coro/thread_pool.hpp>
#include <coro/expected.hpp>

#include <asyncnet/CancellingTask.hpp>

#include <aniparse/Common.hpp>

using namespace aniparse;

CORO_TEST_CASE("gather_awaitables") {
	auto pool = coro::thread_pool::make_shared({});

	auto test_coro = [](std::shared_ptr<coro::thread_pool> pool, std::string val) -> coro::task<std::string> {
		co_await pool->schedule();
		co_return val;
	};

	co_await gather_awaitables();

	auto [r1, r2] = co_await gather_awaitables(
		test_coro(pool, "bla"),
		test_coro(pool, "blabla")
	);
	REQUIRE((r1 == "bla" && r2 == "blabla"));
}

struct TestStruct {
	int value = 4;
};

NetworkRequestTask<TestStruct> test_coro() {
	co_return make_response_error(RequestErrorCode::NotImplemented, "blabla");
}

CORO_TEST_CASE("expected as value") {
	auto result = co_await test_coro();

	REQUIRE(!result.has_value());
	REQUIRE(result.error().code == RequestErrorCode::NotImplemented);
	REQUIRE(result.error().message == "blabla");
}