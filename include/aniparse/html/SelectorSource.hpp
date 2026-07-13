/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/html/CompiledSelector.hpp"

#include "aniparse/utility/AtomicSharedPtr.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace aniparse::html {

/**
 * @brief Data-driven overrides for a parser's CSS selectors, keyed by a stable
 * name (e.g. "example.info.description").
 *
 * A selector set asks the source to compile each selector by name; the source
 * uses the override when present, and otherwise falls back to the set's built-in
 * literal — so an EMPTY source reproduces the built-ins exactly, and a populated
 * one (fed from the volatile catalog/index) hotfixes selectors without an app
 * rebuild. An override that fails to compile (e.g. a bad hotfix off the net)
 * ALSO falls back to the built-in default, which the parser ships and the tests
 * already cover; only the built-in literal is compiled strictly, since a broken
 * literal is a programming error a test must catch loudly.
 */
class SelectorSource {
public:
	/// Shared empty source: every lookup falls back to the built-in literal.
	[[nodiscard]] static const SelectorSource& empty() noexcept;

	SelectorSource() = default;

	/**
	 * @brief Build from a name -> CSS-selector map. Entries are not validated
	 * here; each is checked when compiled, falling back to the default on failure.
	 * @param overrides Selector overrides keyed by their stable name
	 */
	explicit SelectorSource(std::map<std::string, std::string, std::less<>> overrides);

	/**
	 * @brief Compile the selector for @p key, preferring the override.
	 * Uses @p compiler (shared across a set's selectors) to parse the override for
	 * @p key; if there is no override, or it fails to parse, compiles @p fallback
	 * instead — the built-in default. The override goes through non-throwing
	 * parsing so a bad hotfix degrades to the default; @p fallback is compiled
	 * strictly (a broken built-in literal throws, as a test should catch).
	 * @param compiler A selector compiler to reuse for the batch
	 * @param key Stable selector name to look up
	 * @param fallback The set's built-in literal, used when the override is
	 *        missing or invalid
	 * @return The compiled selector (override if valid, else the default)
	 */
	[[nodiscard]] CompiledSelector compile(SelectorCompiler& compiler,
	                                        std::string_view key,
	                                        std::string_view fallback) const;

	/**
	 * @brief The raw override string for @p key if present, else @p fallback.
	 * Unvalidated (may not compile); @ref compile is the safe path. Exposed for
	 * callers that need the selector text rather than a compiled form.
	 */
	[[nodiscard]] std::string_view get(std::string_view key, std::string_view fallback) const;

	/// @return true if the source carries no overrides (every lookup falls back).
	[[nodiscard]] bool empty_source() const noexcept;

private:
	std::map<std::string, std::string, std::less<>> overrides_;
};

/**
 * @brief A swappable slot for the current SelectorSource, so the volatile
 * catalog can hotfix selectors at runtime while requests read lock-free.
 *
 * Holds the source behind an atomic shared_ptr: a reader get()s it (keeping that
 * snapshot alive by refcount even across a concurrent swap), a catalog apply
 * set()s a freshly built one. Never holds null — a default or cleared holder
 * carries an empty source, so every lookup falls back to the built-in literal.
 */
class SelectorSourceHolder {
public:
	SelectorSourceHolder() : source_(std::make_shared<const SelectorSource>()) {}

	explicit SelectorSourceHolder(std::shared_ptr<const SelectorSource> source)
	    : source_(source ? std::move(source) : std::make_shared<const SelectorSource>()) {}

	/// The current source, held alive by the returned handle across a concurrent set().
	[[nodiscard]] std::shared_ptr<const SelectorSource> get() const { return source_.load(); }

	/// Swap in a new source; a null one becomes an empty source.
	void set(std::shared_ptr<const SelectorSource> source) {
		source_.store(source ? std::move(source) : std::make_shared<const SelectorSource>());
	}

private:
	AtomicSharedPtr<const SelectorSource> source_;
};

} // namespace aniparse::html
