/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include <aniparse/utility/Coroutines.hpp>

#include <coro/task.hpp>
#include <coro/expected.hpp>

#include <aniparse/net/CancellingTask.hpp>

#include <aniparse/runtime/RequestContext.hpp>

using namespace aniparse;

struct TestStruct {
	int value = 4;
};

RequestTask<TestStruct> test_coro() {
	co_return make_request_error(RequestErrorCode::NotImplemented, "blabla");
}

CORO_TEST_CASE("expected as value") {
	auto result = co_await test_coro();

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().code == RequestErrorCode::NotImplemented);
	CHECK(result.error().message == "blabla");
}

namespace {
RequestTask<bool> observe_gather_cancellation() {
	co_return (co_await asyncnet::awaitables::get_stop_token).stop_requested();
}

RequestTask<bool> gather_request_results() {
	auto [cancelled, failure] = co_await gather(observe_gather_cancellation(), test_coro());
	// Domain errors remain Result values; only thrown exceptions escape gather.
	REQUIRE_FALSE(failure.has_value());
	CHECK(failure.error().code == RequestErrorCode::NotImplemented);
	co_return std::move(cancelled);
}
}

CORO_TEST_CASE("gather connects RequestTask cancellation and preserves Result errors") {
	auto task = gather_request_results();
	task.request_stop();
	auto result = co_await std::move(task);
	REQUIRE(result.has_value());
	CHECK(*result);
}
