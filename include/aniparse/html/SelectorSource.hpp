/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/html/CompiledSelector.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace aniparse::html {

/**
 * @brief Data-driven overrides for a parser's CSS selectors.
 *
 * A selector set asks the source to compile each selector by name; the source
 * uses the override when present, and otherwise falls back to the set's built-in
 * literal — so an EMPTY source reproduces the built-ins exactly, and a populated
 * one (fed from the volatile catalog/index) hotfixes selectors without an app
 * rebuild. An override that fails to compile (e.g. a bad hotfix off the net)
 * ALSO falls back to the built-in default, which the parser ships and the tests
 * already cover; only the built-in literal is compiled strictly, since a broken
 * literal is a programming error a test must catch loudly.
 *
 * Overrides come in two shapes. The FLAT table (schema v1) maps a fully-spelled
 * stable name (e.g. "example.info.description") to its CSS; every scope sees it.
 * The SCOPED table (schema v2) nests the maps one level deeper — parser
 * identifier -> short key (e.g. "info.description") -> CSS — so one parser's
 * hotfix can never name, let alone clobber, another parser's selector. A set
 * compiled through a scoped view (@ref SelectorSource::scoped_to) reads its
 * parser's short keys; a set compiled against the raw source keeps reading the
 * flat full names, which keeps schema-v1 catalogs and tests working unchanged.
 *
 * The class is subclassable (virtual @ref compile and destructor) so tooling —
 * e.g. the catalog dumper — can RECORD the (key, fallback) pairs a set declares
 * while delegating to the base implementation. A subclass MUST preserve the
 * fallback-first semantics: an override that changes which text wins (serving
 * anything but the override-or-fallback the base would pick) would silently
 * break the hotfix contract the parsers and tests rely on.
 */
class SelectorSource {
public:
	/// Flat table: fully-spelled selector name -> override CSS (schema v1 shape).
	using Overrides = std::map<std::string, std::string, std::less<>>;
	/// Scoped table: parser identifier -> (short selector key -> override CSS).
	using ScopedOverrides = std::map<std::string, Overrides, std::less<>>;

	/// Shared empty source: every lookup falls back to the built-in literal.
	[[nodiscard]] static const SelectorSource& empty() noexcept;

	SelectorSource() = default;

	/**
	 * @brief Destroy the source. Virtual so recording/observing subclasses (e.g.
	 * the catalog dumper's) can be destroyed through a base pointer.
	 */
	virtual ~SelectorSource() = default;

	/**
	 * @brief Build from a flat name -> CSS-selector map (schema v1).
	 * Entries are not validated here; each is checked when compiled, falling back
	 * to the default on failure.
	 * @param overrides Selector overrides keyed by their fully-spelled stable name
	 */
	explicit SelectorSource(Overrides overrides);

	/**
	 * @brief Build from the schema-v2 scoped table plus an optional flat one.
	 * @param shared   Flat overrides visible to every scope (legacy full names)
	 * @param scoped   Per-parser overrides keyed by parser identifier, then by
	 *        the set's short selector key
	 */
	SelectorSource(Overrides shared, ScopedOverrides scoped);

	/**
	 * @brief Compile the selector for @p key, preferring the flat override.
	 * Uses @p compiler (shared across a set's selectors) to parse the override for
	 * @p key; if there is no override, or it fails to parse, compiles @p fallback
	 * instead — the built-in default. The override goes through non-throwing
	 * parsing so a bad hotfix degrades to the default; @p fallback is compiled
	 * strictly (a broken built-in literal throws, as a test should catch).
	 * @param compiler A selector compiler to reuse for the batch
	 * @param key Stable selector name to look up in the flat table
	 * @param fallback The set's built-in literal, used when the override is
	 *        missing or invalid
	 * @return The compiled selector (override if valid, else the default)
	 * @note Virtual: subclasses may record or observe the (key, fallback) pair,
	 *       but MUST delegate the decision to this base implementation — the
	 *       fallback-first semantics are the hotfix contract.
	 */
	[[nodiscard]] virtual CompiledSelector compile(SelectorCompiler& compiler,
	                                                std::string_view key,
	                                                std::string_view fallback) const;

	/**
	 * @brief The raw override string for @p key if present, else @p fallback.
	 * Unvalidated (may not compile); @ref compile is the safe path. Exposed for
	 * callers that need the selector text rather than a compiled form.
	 * @param key Stable selector name to look up in the flat table
	 * @param fallback The text to return when the key carries no override
	 * @return The override text, or @p fallback
	 */
	[[nodiscard]] std::string_view get(std::string_view key, std::string_view fallback) const;

	/// @return true if neither table carries overrides (every lookup falls back).
	[[nodiscard]] bool empty_source() const noexcept;

	/**
	 * @brief Whether the scoped table holds any override for @p parser_id.
	 * @param parser_id The parser identifier to check
	 * @return True when a scoped view for this id would see more than the flat table
	 */
	[[nodiscard]] bool has_scope(std::string_view parser_id) const;

	/**
	 * @brief Build the source one parser sees: its scoped overrides merged over
	 * the flat table, as a new flat source.
	 *
	 * The result is a materialized snapshot (maps are copied), so the caller may
	 * outlive — or be cached independently of — the source it was built from. A
	 * scoped entry wins a flat one on the same key; in practice the two never
	 * collide, since flat keys are fully-spelled names and scoped keys are short.
	 * @param parser_id Parser identifier to scope to; an empty id yields just the
	 *        flat table
	 * @return A flat source holding exactly what this parser may see
	 */
	[[nodiscard]] std::shared_ptr<const SelectorSource> scoped_to(std::string_view parser_id) const;

protected:
	SelectorSource(const SelectorSource&) = default;
	SelectorSource(SelectorSource&&) = default;
	SelectorSource& operator=(const SelectorSource&) = default;
	SelectorSource& operator=(SelectorSource&&) = default;

private:
	Overrides overrides_;
	ScopedOverrides scoped_;
};

} // namespace aniparse::html
