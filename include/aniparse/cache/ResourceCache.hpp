/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

namespace aniparse::detail {

/// One distinct byte per type T; its address is a program-unique constant.
template <class T>
inline constexpr char resource_type_tag{};

/// Stable, RTTI-free key per resource-set type: the address of resource_type_tag<T>.
template <class T>
inline const void* resource_type_key() noexcept {
	return &resource_type_tag<T>;
}

} // namespace aniparse::detail

namespace aniparse {

/**
 * @brief Lazily built, type-keyed store of compiled parser resources.
 * Holds any resource set type T keyed by its own type, built once on first use
 * and reused afterwards. Not tied to any resource kind: a set can be compiled
 * CSS selectors, compiled regexes, or anything else a parser precomputes.
 *
 * A set provides how to build itself with a static factory:
 * @code
 * struct CardSelectors {
 *     html::CompiledSelector card, link;
 *     static CardSelectors create() {
 *         html::SelectorCompiler c;                 // transient, one per set
 *         return { c.compile(".card"), c.compile(".card > a") };
 *     }
 * };
 * auto sel = cache.get<CardSelectors>();            // compiled once, then cached
 * @endcode
 *
 * @note Thread safe. @ref get returns a shared_ptr so a concurrent
 *       @ref evict / @ref clear cannot destroy a set that is still in use.
 */
class ResourceCache {
public:
	ResourceCache() = default;

	ResourceCache(const ResourceCache&)            = delete;
	ResourceCache& operator=(const ResourceCache&) = delete;

	/**
	 * @brief Get resource set T, building it once on first request via T::create().
	 * @tparam T Resource set type exposing `static T create()`.
	 * @return Shared handle to the built set. Hold it for the whole operation so
	 *         a concurrent evict/clear cannot free it underneath.
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> get() {
		return get<T>([] { return T::create(); });
	}

	/**
	 * @brief Get resource set T, building it with @p factory on a cache miss.
	 * @tparam T Resource set type; the key is T regardless of the factory.
	 * @param factory Invocable returning a T; called at most once, only on miss.
	 * @return Shared handle to the built (or already cached) set.
	 */
	template <class T, class Factory>
	[[nodiscard]] std::shared_ptr<const T> get(Factory&& factory) {
		const void* key = detail::resource_type_key<T>();

		{
			std::shared_lock read_lock(mutex_);
			auto it = resources_.find(key);
			if (it != resources_.end()) {
				return std::static_pointer_cast<const T>(it->second);
			}
		}

		// Build outside the lock: it is the expensive step and needs no shared
		// state. Losing a build race just drops our copy for the stored winner.
		std::shared_ptr<const T> built = std::make_shared<const T>(std::forward<Factory>(factory)());

		std::unique_lock write_lock(mutex_);
		auto [it, inserted] = resources_.try_emplace(key, std::move(built));
		return std::static_pointer_cast<const T>(it->second);
	}

	/**
	 * @brief Drop the cached set T, if present.
	 * @note Handles previously returned by @ref get keep their set alive until
	 *       released; a later @ref get rebuilds it.
	 */
	template <class T>
	void evict() {
		std::unique_lock write_lock(mutex_);
		resources_.erase(detail::resource_type_key<T>());
	}

	/**
	 * @brief Drop all cached sets (e.g. under memory pressure).
	 * @note In-flight handles from @ref get survive until released.
	 */
	void clear() {
		std::unique_lock write_lock(mutex_);
		resources_.clear();
	}

private:
	std::shared_mutex mutex_;
	std::unordered_map<const void*, std::shared_ptr<const void>> resources_;
};

} // namespace aniparse
