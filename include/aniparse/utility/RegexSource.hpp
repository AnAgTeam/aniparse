/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/cache/ResourceCache.hpp"
#include "aniparse/utility/AtomicSharedPtr.hpp"
#include "aniparse/utility/Regex.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>

namespace aniparse {

/**
 * @brief Data-driven overrides for a parser's or extractor's regex patterns.
 *
 * A pattern set asks the source to compile each pattern by name; the source uses
 * the override when present, and otherwise falls back to the set's built-in
 * literal — so an EMPTY source reproduces the built-ins exactly, and a populated
 * one (fed from the volatile catalog) hotfixes patterns without an app rebuild.
 *
 * Two failure shapes degrade an override to the built-in default instead of
 * breaking the consumer:
 *  - the override text does not compile (boost::regex_error);
 *  - the override does not declare a NAMED GROUP the built-in literal declares
 *    (a group the consumer reads back by name from the match). The contracted
 *    set is derived from the built-in literal itself — the literal is the
 *    single source of truth, there is no separate declaration to keep in sync.
 *    The check is textual (`(?<name>` / `(?'name'` / `(?P<name>`), because a
 *    compiled regex does not expose its group names.
 * The built-in literal itself is compiled strictly: a broken default is a
 * programming error a test must catch loudly, not a runtime condition.
 *
 * The table is SCOPED: owner identifier (parser id or extractor identifier) ->
 * short key (e.g. "info.title") -> pattern text, so one owner's hotfix can never
 * name, let alone clobber, another owner's pattern. A set compiled through a
 * scoped view (@ref RegexSourceHolder::view_for) reads only its owner's keys.
 *
 * The class is subclassable (virtual @ref compile and destructor) so tooling —
 * e.g. the catalog dumper — can RECORD the (key, fallback) pairs a set declares
 * while delegating to the base implementation. A subclass MUST preserve the
 * fallback-first semantics: an override that changes which text wins (serving
 * anything but the override-or-fallback the base would pick) would silently
 * break the hotfix contract the parsers and tests rely on.
 */
class RegexSource {
public:
	/// Flat table of one owner's patterns: short key -> override pattern. This is
	/// the shape a scoped view materializes into.
	using Overrides = std::map<std::string, std::string, std::less<>>;
	/// Scoped table: owner identifier -> (short pattern key -> override pattern).
	using ScopedOverrides = std::map<std::string, Overrides, std::less<>>;

	/// Shared empty source: every lookup falls back to the built-in literal.
	[[nodiscard]] static const RegexSource& empty() noexcept;

	RegexSource() = default;

	/**
	 * @brief Destroy the source. Virtual so recording/observing subclasses (e.g.
	 * the catalog dumper's) can be destroyed through a base pointer.
	 */
	virtual ~RegexSource() = default;

	/**
	 * @brief Build an already-scoped (flat) source, e.g. the result of scoped_to().
	 * Entries are not validated here; each is checked when compiled, falling back
	 * to the default on failure.
	 * @param overrides Pattern overrides keyed by their short key
	 */
	explicit RegexSource(Overrides overrides);

	/**
	 * @brief Build from the scoped table.
	 * @param scoped Per-owner overrides keyed by owner identifier, then by the
	 *        set's short pattern key
	 */
	explicit RegexSource(ScopedOverrides scoped);

	/**
	 * @brief Compile the pattern for @p key, preferring the override.
	 *
	 * When an override exists AND declares every named group @p fallback declares
	 * AND compiles, it wins; any failure along that chain falls back to
	 * @p fallback (the built-in literal), which is then compiled strictly. The
	 * named-group contract is derived from @p fallback itself: whatever groups
	 * the built-in declares are the ones the consumer may read back by name, so
	 * an override must declare the same set.
	 * @param key Stable pattern name to look up in the flat table
	 * @param fallback The set's built-in literal, used when the override is
	 *        missing, group-incomplete, or invalid
	 * @return The compiled pattern (override if valid and complete, else the default)
	 * @throws boost::regex_error when @p fallback itself fails to compile.
	 * @note An invalid OVERRIDE never throws; it degrades to the built-in.
	 * @note Virtual: subclasses may record or observe the (key, fallback) pair,
	 *       but MUST delegate the decision to this base implementation — the
	 *       fallback-first semantics are the hotfix contract.
	 */
	[[nodiscard]] virtual regex compile(std::string_view key, std::string_view fallback) const;

	/**
	 * @brief The raw override string for @p key if present, else @p fallback.
	 * Unvalidated (may not compile or declare the required groups); @ref compile
	 * is the safe path. Exposed for callers that need the pattern text rather
	 * than a compiled form.
	 * @param key Stable pattern name to look up in the flat table
	 * @param fallback The text to return when the key carries no override
	 * @return The override text, or @p fallback
	 */
	[[nodiscard]] std::string_view get(std::string_view key, std::string_view fallback) const;

	/// @return true if the source carries no overrides (every lookup falls back).
	[[nodiscard]] bool empty_source() const noexcept;

	/**
	 * @brief Whether the scoped table holds any override for @p owner.
	 * @param owner The owner identifier to check
	 * @return True when a scoped view for this owner would see any override
	 */
	[[nodiscard]] bool has_scope(std::string_view owner) const;

	/**
	 * @brief Build the source one owner sees: its scoped overrides as a new flat
	 * source.
	 *
	 * The result is a materialized snapshot (the map is copied), so the caller may
	 * outlive — or be cached independently of — the source it was built from.
	 * @param owner Owner identifier to scope to; an unknown id yields an empty
	 *        flat source
	 * @return A flat source holding exactly what this owner may see
	 */
	[[nodiscard]] std::shared_ptr<const RegexSource> scoped_to(std::string_view owner) const;

private:
	Overrides overrides_;
	ScopedOverrides scoped_;
};

/**
 * @brief A swappable slot for the current RegexSource, so the volatile catalog
 * can hotfix patterns at runtime while requests read lock-free.
 *
 * Holds the source behind an atomic shared_ptr: a reader get()s it (keeping that
 * snapshot alive by refcount even across a concurrent swap), a catalog apply
 * set()s a freshly built one. Never holds null — a default or cleared holder
 * carries an empty source, so every lookup falls back to the built-in literal.
 *
 * The holder also owns the compiled-pattern-set cache, keyed by (owner id, set
 * type): a pattern set is the one resource that legitimately shares its TYPE
 * across owners (an engine family's common set) while its CONTENT depends on the
 * owner's own overrides. The cache is dropped on every set(), so a catalog apply
 * recompiles against the new overrides.
 */
class RegexSourceHolder {
public:
	RegexSourceHolder() : source_(std::make_shared<const RegexSource>()) {}

	explicit RegexSourceHolder(std::shared_ptr<const RegexSource> source)
	    : source_(source ? std::move(source) : std::make_shared<const RegexSource>()) {}

	/// The current source, held alive by the returned handle across a concurrent set().
	[[nodiscard]] std::shared_ptr<const RegexSource> get() const { return source_.load(); }

	/**
	 * @brief Swap in a new source; a null one becomes an empty source.
	 * Drops every cached scoped view and compiled set: they were built from the
	 * old source's overrides. In-flight readers keep their snapshots alive by
	 * refcount; later builds see the new source.
	 */
	void set(std::shared_ptr<const RegexSource> source) {
		source_.store(source ? std::move(source) : std::make_shared<const RegexSource>());
		std::unique_lock write_lock(mutex_);
		views_.clear();
		sets_.clear();
	}

	/**
	 * @brief The source scoped to one owner, cached until the next set().
	 * @param owner The owner identifier (a parser id stamped into a context's
	 *        config, or an extractor's identifier()); an empty id or an id the
	 *        source carries no scoped overrides for yields the shared source itself
	 * @return The scoped source view
	 */
	[[nodiscard]] std::shared_ptr<const RegexSource> view_for(std::string_view owner) {
		auto source = source_.load();
		if (owner.empty() || !source->has_scope(owner)) {
			return source;
		}
		{
			std::shared_lock read_lock(mutex_);
			if (auto it = views_.find(owner); it != views_.end()) {
				return it->second;
			}
		}
		// Build outside the lock: a losing race just drops our copy for the winner.
		auto view = source->scoped_to(owner);
		std::unique_lock write_lock(mutex_);
		auto [it, inserted] = views_.try_emplace(std::string(owner), std::move(view));
		return it->second;
	}

	/**
	 * @brief Build (once per owner id, cached) the pattern set @p T from that
	 * owner's scoped source view.
	 * @tparam T Pattern set type exposing `static T create(const RegexSource&)`
	 * @param owner The owner identifier to scope overrides to
	 * @return Shared handle to the built set; hold it for the whole operation so a
	 *         concurrent set() cannot free it underneath
	 * @note The cache key is (owner id, T): two owners sharing one set type get
	 *       one compiled copy EACH, with their own overrides applied.
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> set_for(std::string_view owner) {
		const void* type_key = detail::resource_type_key<T>();
		{
			std::shared_lock read_lock(mutex_);
			if (auto by_id = sets_.find(owner); by_id != sets_.end()) {
				if (auto it = by_id->second.find(type_key); it != by_id->second.end()) {
					return std::static_pointer_cast<const T>(it->second);
				}
			}
		}
		// Compile outside the lock, off the scoped view: expensive and in need of
		// no shared state. A losing build race drops our copy for the stored winner.
		auto view = view_for(owner);
		auto built = std::make_shared<const T>(T::create(*view));
		std::unique_lock write_lock(mutex_);
		auto [it, inserted] = sets_[std::string(owner)].try_emplace(type_key, std::move(built));
		return std::static_pointer_cast<const T>(it->second);
	}

private:
	AtomicSharedPtr<const RegexSource> source_;
	std::shared_mutex mutex_;
	std::map<std::string, std::shared_ptr<const RegexSource>, std::less<>> views_;
	std::map<std::string, std::map<const void*, std::shared_ptr<const void>>, std::less<>> sets_;
};

} // namespace aniparse
