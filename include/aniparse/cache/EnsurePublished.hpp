/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/cache/Republishable.hpp"
#include "aniparse/utility/Result.hpp"

#include <memory>
#include <utility>

namespace aniparse {

/**
 * @brief Warm a @ref Republishable once, then serve its snapshot: cache-first.
 *
 * On a warm cell (@ref Republishable::is_fetched) returns the snapshot without
 * fetching. On a cold cell runs @p fetch; on success publishes and returns the
 * new snapshot; on failure propagates the error and leaves the cell cold so a
 * later call retries. Concurrent callers share that attempt, including its error.
 *
 * @tparam T     Payload type of the cell.
 * @param cell   Cache handle, taken by value so it is kept alive for the whole
 *               call — a concurrent evict/clear cannot free the cell across the
 *               co_await.
 * @param fetch  Invocable returning `RequestTask<std::shared_ptr<const T>>`;
 *               copied into the coroutine frame and called at most once, only on a
 *               cold cell. A lambda whose body calls non-const RequestContext
 *               methods (request / request_json / ...) must be declared `mutable`,
 *               since its captured context copy is otherwise const.
 *
 * @note The first caller owns the fetch and supplies its cancellation context.
 *       Cancelling or destroying that caller ends the shared attempt; destruction
 *       wakes its waiters with Cancelled. A waiting caller's stop request does not
 *       cancel the fetch; it is observed when the attempt completes. Destroying a
 *       suspended waiter unregisters it. As with other tasks, callers must not
 *       destroy a task concurrently with its execution or resumption.
 * @note Fetch exceptions are rethrown to all participants. Waiters resume on the
 *       completing thread, without blocking a worker thread while waiting.
 */
template <class T, class Fetch>
RequestTask<std::shared_ptr<const T>>
ensure_published(std::shared_ptr<const Republishable<T>> cell, Fetch fetch) {
    auto stop = co_await asyncnet::awaitables::get_stop_token;
    if (stop.stop_requested()) {
        co_return make_request_error(RequestErrorCode::Cancelled, "Cache load cancelled");
    }

    using Flight = typename Republishable<T>::Flight;
    std::shared_ptr<Flight> flight;
    bool leader = false;
    {
        std::lock_guard lock(cell->flight_mutex_);
        if (cell->is_fetched()) co_return cell->snapshot();
        if (!cell->flight_) {
            cell->flight_ = std::make_shared<Flight>();
            leader = true;
        }
        flight = cell->flight_;
    }

    if (leader) {
        typename Republishable<T>::FlightOwner owner{*cell, flight};
        try {
            auto built = co_await fetch();
            if (stop.stop_requested()) {
                flight->outcome = make_request_error(RequestErrorCode::Cancelled, "Cache load cancelled");
            } else {
                flight->outcome = std::move(built);
                if (flight->outcome) cell->publish(*flight->outcome);
            }
        } catch (...) {
            flight->exception = std::current_exception();
        }
        owner.finish();
    } else {
        co_await typename Flight::Waiter{flight};
    }
    if (stop.stop_requested()) {
        co_return make_request_error(RequestErrorCode::Cancelled, "Cache load cancelled");
    }
    co_return flight->result();
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
RequestTask<std::shared_ptr<const T>>
ensure_published_or_seed(std::shared_ptr<const Republishable<T>> cell, Fetch fetch) {
    auto result = co_await ensure_published(cell, std::move(fetch));
    if (result) co_return *result;
    co_return cell->snapshot(); // Seed fallback does not mark a failed attempt fetched.
}

} // namespace aniparse
