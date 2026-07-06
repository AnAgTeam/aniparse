/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/html/CompiledSelector.hpp"

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace aniparse::html {

/**
 * @brief Data-driven overrides for a parser's CSS selectors, keyed by a stable
 * name (e.g. "henchan.info.description").
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

} // namespace aniparse::html
