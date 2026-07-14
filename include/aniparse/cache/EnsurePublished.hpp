/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/cache/Republishable.hpp"
#include "aniparse/types/Response.hpp"

#include <memory>
#include <utility>

namespace aniparse {

/**
 * @brief Warm a @ref Republishable once, then serve its snapshot: cache-first.
 *
 * On a warm cell (@ref Republishable::is_fetched) returns the snapshot without
 * fetching. On a cold cell runs @p fetch; on success publishes and returns the
 * new snapshot; on failure propagates the error and leaves the cell cold so a
 * later call retries.
 *
 * @tparam T     Payload type of the cell.
 * @param cell   Cache handle, taken by value so it is kept alive for the whole
 *               call — a concurrent evict/clear cannot free the cell across the
 *               co_await.
 * @param fetch  Invocable returning `NetworkRequestTask<std::shared_ptr<const T>>`;
 *               copied into the coroutine frame and called at most once, only on a
 *               cold cell. A lambda whose body calls non-const RequestorContext
 *               methods (request / request_json / ...) must be declared `mutable`,
 *               since its captured context copy is otherwise const.
 *
 * @note Cache-first is best-effort, not once-only: two callers racing a cold cell
 *       may both fetch and both publish (the last store wins). Correct, just
 *       redundant work — add an in-flight guard if strict single-fetch is needed.
 */
template <class T, class Fetch>
NetworkRequestTask<std::shared_ptr<const T>>
ensure_published(std::shared_ptr<const Republishable<T>> cell, Fetch fetch) {
	if (cell->is_fetched()) {
		co_return cell->snapshot();
	}
	Response<std::shared_ptr<const T>> built = co_await fetch();
	if (!built) {
		co_return unexpected(std::move(built.error()));
	}
	cell->publish(*built);
	co_return *built;
}

/**
 * @brief Like @ref ensure_published, but on fetch failure serves the seed instead
 *        of propagating the error, leaving the cell cold so a later call retries.
 *
 * For callers whose @ref Republishable was seeded with a usable value (e.g. a
 * default image host) that keeps the parser working before the first warm. Never
 * yields an error from the fetch itself; a null seed simply yields a null snapshot.
 */
template <class T, class Fetch>
NetworkRequestTask<std::shared_ptr<const T>>
ensure_published_or_seed(std::shared_ptr<const Republishable<T>> cell, Fetch fetch) {
	if (cell->is_fetched()) {
		co_return cell->snapshot();
	}
	if (Response<std::shared_ptr<const T>> built = co_await fetch()) {
		cell->publish(*built);
		co_return *built;
	}
	co_return cell->snapshot(); // seed; cell stays cold -> a later call retries
}

} // namespace aniparse
