/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/html/CompiledSelector.hpp"
#include "aniparse/cache/ResourceCache.hpp"

#include "aniparse/utility/AtomicSharedPtr.hpp"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
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
 * compiled through a scoped view (@ref SelectorSourceHolder::view_for) reads its
 * parser's short keys; a set compiled against the raw source keeps reading the
 * flat full names, which keeps schema-v1 catalogs and tests working unchanged.
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
	 */
	[[nodiscard]] CompiledSelector compile(SelectorCompiler& compiler,
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

private:
	Overrides overrides_;
	ScopedOverrides scoped_;
};

/**
 * @brief A swappable slot for the current SelectorSource, so the volatile
 * catalog can hotfix selectors at runtime while requests read lock-free.
 *
 * Holds the source behind an atomic shared_ptr: a reader get()s it (keeping that
 * snapshot alive by refcount even across a concurrent swap), a catalog apply
 * set()s a freshly built one. Never holds null — a default or cleared holder
 * carries an empty source, so every lookup falls back to the built-in literal.
 *
 * The holder also owns the compiled-selector-set cache, keyed by (parser id, set
 * type): a selector set is the one resource that legitimately shares its TYPE
 * across parsers (an engine family's common set) while its CONTENT depends on
 * the parser's own overrides, so it lives here rather than in the type-keyed
 * ResourceCache, where the first family member to build would win for all. The
 * cache is dropped on every set(), so a catalog apply recompiles against the
 * new overrides.
 */
class SelectorSourceHolder {
public:
	SelectorSourceHolder() : source_(std::make_shared<const SelectorSource>()) {}

	explicit SelectorSourceHolder(std::shared_ptr<const SelectorSource> source)
	    : source_(source ? std::move(source) : std::make_shared<const SelectorSource>()) {}

	/// The current source, held alive by the returned handle across a concurrent set().
	[[nodiscard]] std::shared_ptr<const SelectorSource> get() const { return source_.load(); }

	/**
	 * @brief Swap in a new source; a null one becomes an empty source.
	 * Drops every cached scoped view and compiled set: they were built from the
	 * old source's overrides. In-flight readers keep their snapshots alive by
	 * refcount; later builds see the new source.
	 */
	void set(std::shared_ptr<const SelectorSource> source) {
		source_.store(source ? std::move(source) : std::make_shared<const SelectorSource>());
		std::unique_lock write_lock(mutex_);
		views_.clear();
		sets_.clear();
	}

	/**
	 * @brief The source scoped to one parser, cached until the next set().
	 * @param parser_id The parser identifier stamped into a context's config; an
	 *        empty id (or an id the source carries no scoped overrides for) yields
	 *        the shared source itself
	 * @return The scoped source view
	 */
	[[nodiscard]] std::shared_ptr<const SelectorSource> view_for(std::string_view parser_id) {
		auto source = source_.load();
		if (parser_id.empty() || !source->has_scope(parser_id)) {
			return source;
		}
		{
			std::shared_lock read_lock(mutex_);
			if (auto it = views_.find(parser_id); it != views_.end()) {
				return it->second;
			}
		}
		// Build outside the lock: a losing race just drops our copy for the winner.
		auto view = source->scoped_to(parser_id);
		std::unique_lock write_lock(mutex_);
		auto [it, inserted] = views_.try_emplace(std::string(parser_id), std::move(view));
		return it->second;
	}

	/**
	 * @brief Build (once per parser id, cached) the selector set @p T from that
	 * parser's scoped source view.
	 * @tparam T Selector set type exposing `static T create(const SelectorSource&)`
	 * @param parser_id The parser identifier stamped into the calling context
	 * @return Shared handle to the built set; hold it for the whole operation so a
	 *         concurrent set() cannot free it underneath
	 * @note The cache key is (parser id, T): two parsers sharing one set type get
	 *       one compiled copy EACH, with their own overrides applied.
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> set_for(std::string_view parser_id) {
		const void* type_key = detail::resource_type_key<T>();
		{
			std::shared_lock read_lock(mutex_);
			if (auto by_id = sets_.find(parser_id); by_id != sets_.end()) {
				if (auto it = by_id->second.find(type_key); it != by_id->second.end()) {
					return std::static_pointer_cast<const T>(it->second);
				}
			}
		}
		// Compile outside the lock, off the scoped view: expensive and in need of
		// no shared state. A losing build race drops our copy for the stored winner.
		auto view = view_for(parser_id);
		auto built = std::make_shared<const T>(T::create(*view));
		std::unique_lock write_lock(mutex_);
		auto [it, inserted] = sets_[std::string(parser_id)].try_emplace(type_key, std::move(built));
		return std::static_pointer_cast<const T>(it->second);
	}

private:
	AtomicSharedPtr<const SelectorSource> source_;
	std::shared_mutex mutex_;
	std::map<std::string, std::shared_ptr<const SelectorSource>, std::less<>> views_;
	std::map<std::string, std::map<const void*, std::shared_ptr<const void>>, std::less<>> sets_;
};

} // namespace aniparse::html
