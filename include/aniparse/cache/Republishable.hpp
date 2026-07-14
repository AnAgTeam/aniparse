/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/AtomicSharedPtr.hpp"

#include <memory>
#include <utility>

namespace aniparse {

/**
 * @brief A cache value that serves an immutable seed until a fetched snapshot is
 *        published, then serves the snapshot — atomically, via @ref AtomicSharedPtr.
 *
 * Meant to live in @ref ResourceCache as a plain value: cached by type, built
 * once via @ref create (or a factory), handed out as a const handle. That const
 * handle can still @ref publish a refreshed snapshot because the payload sits
 * behind an @ref AtomicSharedPtr cell reached through the (logically const) object.
 *
 * One cell, not a value plus a separate fetched flag: "already fetched" is
 * encoded as "the cell holds a snapshot" (@ref is_fetched), so publishing the
 * data and flipping the fetched state is a single store — there is no window
 * where one is visible without the other. A @p seed passed at construction is
 * served by @ref snapshot before the first publish, so readers get a usable value
 * on a cold cache (e.g. a default image host) instead of nothing.
 *
 * @tparam T Payload type; snapshots are shared as `std::shared_ptr<const T>`.
 *
 * @code
 * // Empty seed: snapshot() is null until warmed; fetch gates on is_fetched().
 * auto v = ctx.resources().get<Republishable<std::string>>();
 *
 * // Seeded: snapshot() returns the seed until the first publish. A seed needs a
 * // build argument, so use the factory overload of get:
 * auto c = ctx.resources().get<Republishable<CatalogData>>(
 *     [] { return Republishable<CatalogData>{make_seed()}; });
 * @endcode
 *
 * @note Thread safe. Concurrent @ref publish / @ref snapshot are safe (the cell's
 *       store/load are mutex-guarded); a reader holds the returned `shared_ptr`,
 *       so a concurrent publish cannot free the snapshot it is reading. Keep the
 *       enclosing cache handle alive for the whole read so a concurrent
 *       evict/clear cannot free the object. Swaps are rare (warm/refresh), so the
 *       cell's lock is not a hot path.
 */
template <class T>
class Republishable {
public:
	Republishable() = default;

	/// Construct with the value @ref snapshot serves until the first @ref publish.
	explicit Republishable(std::shared_ptr<const T> seed) : seed_(std::move(seed)) {}

	/// A move only ever runs while installing into the cache, on a temporary that
	/// no other thread can reach, so reading @p other's cell here is safe. The cell
	/// (@ref AtomicSharedPtr) is itself non-movable, so this rebuilds a fresh cell
	/// from the loaded snapshot rather than moving it. Declaring this ctor also
	/// makes the type non-copyable, which is intended: the value is shared through
	/// the cache handle, never copied.
	Republishable(Republishable&& other) noexcept
	    : seed_(std::move(other.seed_)), cell_(other.cell_.load()) {}

	/// Factory for the no-argument `ResourceCache::get`: an empty seed. Seeded
	/// values are built through the factory overload of `get` (see the class docs).
	static Republishable create() { return Republishable{}; }

	/// The published snapshot if one exists, otherwise the construction seed (which
	/// may itself be null when none was given). Never blocks meaningfully.
	[[nodiscard]] std::shared_ptr<const T> snapshot() const {
		std::shared_ptr<const T> current = cell_.load();
		return current ? current : seed_;
	}

	/// True once a snapshot has been published; the fetch-once gate for callers.
	[[nodiscard]] bool is_fetched() const { return cell_.load() != nullptr; }

	/// Publish a fresh snapshot, visible to later @ref snapshot / @ref is_fetched.
	/// Const because the cache hands out a const handle; the cell is the one seam
	/// through which an otherwise-immutable cached value is allowed to change.
	void publish(std::shared_ptr<const T> value) const { cell_.store(std::move(value)); }

private:
	/// Immutable after construction: served before the first publish. A plain
	/// member (not a cell) — never written once built, so concurrent reads are safe.
	std::shared_ptr<const T> seed_;

	/// The published snapshot; null until the first @ref publish. `mutable` so
	/// @ref publish can store through the const cache handle (logical constness).
	mutable AtomicSharedPtr<const T> cell_;
};

} // namespace aniparse
