/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <memory>
#include <mutex>

namespace aniparse {

/**
 * @brief Portable stand-in for std::atomic<std::shared_ptr<T>> (P0718).
 *
 * Apple's libc++ does not provide the atomic<shared_ptr> specialization, so the
 * primary std::atomic<T> template is selected and static_asserts on
 * !is_trivially_copyable. This wrapper offers just the load/store subset the
 * codebase uses, guarded by a mutex. Semantics match the intended use: load()
 * returns a ref-counted snapshot that outlives a concurrent store(); readers
 * already holding a handle keep their snapshot alive. The swaps here are rare
 * (catalog/session/selector refresh), so the mutex is not a hot path.
 *
 * The implementation is pinned to this mutex-guarded form on every platform
 * (never std::conditional-switched to the native atomic<shared_ptr> where it
 * exists), so the mangled type stays identical across targets and can appear in
 * the public interface of parsers without an ABI split.
 */
template <typename T>
class AtomicSharedPtr {
public:
	AtomicSharedPtr() = default;
	AtomicSharedPtr(std::shared_ptr<T> ptr) : ptr_(std::move(ptr)) {}

	AtomicSharedPtr(const AtomicSharedPtr&)            = delete;
	AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

	[[nodiscard]] std::shared_ptr<T> load() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return ptr_;
	}

	void store(std::shared_ptr<T> ptr) {
		std::lock_guard<std::mutex> lock(mutex_);
		ptr_ = std::move(ptr);
	}

private:
	mutable std::mutex mutex_;
	std::shared_ptr<T> ptr_;
};

} // namespace aniparse
