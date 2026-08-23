/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/html/SelectorSource.hpp"
#include "aniparse/utility/Format.hpp"
#include "aniparse/utility/RegexSource.hpp"

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aniparse {

/**
 * @brief A SelectorSource that RECORDS every (key, fallback) pair a set
 * declares, delegating the compile to the base class.
 *
 * Used by catalog tooling (CatalogSink): building a set through it captures
 * the set's built-in literals while STRICTLY compiling each through the real
 * engine — a broken literal is a programming error and propagates, annotated
 * with the key and the offending text.
 * @note The fallback-first semantics of the base class are preserved exactly;
 *       recording is observation, not a behavior change.
 */
class RecordingSelectorSource : public html::SelectorSource {
public:
	/// The recorded short key -> built-in literal table, sorted by key.
	using RecordedSet = std::map<std::string, std::string, std::less<>>;

	/**
	 * @brief Record the (key, fallback) pair, then compile exactly as the base.
	 * @param compiler A selector compiler to reuse for the batch.
	 * @param key Stable selector name the set declares.
	 * @param fallback The set's built-in literal.
	 * @return The compiled selector, as the base class would produce it.
	 * @throws html::SelectorParseError when the built-in literal fails to compile,
	 *         annotated with the key and the offending text.
	 */
	[[nodiscard]] html::CompiledSelector compile(html::SelectorCompiler& compiler,
	                                             std::string_view key,
	                                             std::string_view fallback) const override {
		try {
			recorded.emplace(key, fallback);
			return SelectorSource::compile(compiler, key, fallback);
		} catch (const html::SelectorParseError& error) {
			throw html::SelectorParseError(fmt::format(
			    "broken built-in selector '{}': \"{}\" ({})", key, fallback, error.what()));
		}
	}

	/// Every (key, fallback) pair recorded so far; valid to read after T::create.
	mutable RecordedSet recorded;
};

/**
 * @brief A RegexSource that RECORDS every (key, fallback) pair a set declares,
 * delegating the compile to the base class. The pattern-set counterpart of
 * @ref RecordingSelectorSource.
 * @note The fallback-first semantics (including the named-group contract) of
 *       the base class are preserved exactly.
 */
class RecordingRegexSource : public RegexSource {
public:
	/// The recorded short key -> built-in literal table, sorted by key.
	using RecordedSet = std::map<std::string, std::string, std::less<>>;

	/**
	 * @brief Record the (key, fallback) pair, then compile exactly as the base.
	 * @param key Stable pattern name the set declares.
	 * @param fallback The set's built-in literal.
	 * @return The compiled pattern, as the base class would produce it.
	 * @throws boost::regex_error when the built-in literal fails to compile,
	 *         annotated with the key and the offending text.
	 */
	[[nodiscard]] regex compile(std::string_view key, std::string_view fallback) const override {
		try {
			recorded.emplace(key, fallback);
			return RegexSource::compile(key, fallback);
		} catch (const boost::regex_error& error) {
			throw boost::regex_error(fmt::format(
			    "broken built-in pattern '{}': \"{}\" ({})", key, fallback, error.what()));
		}
	}

	/// Every (key, fallback) pair recorded so far; valid to read after T::create.
	mutable RecordedSet recorded;
};

namespace detail {
template <class>
inline constexpr bool always_false = false;
} // namespace detail

/**
 * @brief The sink a parser or extractor declares its catalog-visible defaults
 * into via `emplace_catalog()`.
 *
 * One sink serves one owner (a parser or extractor identifier); the catalog
 * dumper collects the recorded data afterwards. Two channels are declarative:
 * the owner's selector/pattern SETS (@ref emplace_set) and its canonical base
 * URL (@ref set_canonical_base_url). Both are optional: an owner that declares
 * nothing simply has no hotfix channel and no canonical URL on the wire.
 *
 * The sink is valid only for the duration of the emplace_catalog call — an
 * owner must not retain it.
 */
class CatalogSink {
public:
	/// The recorded short key -> built-in literal table of one channel.
	using RecordedSet = std::map<std::string, std::string, std::less<>>;

	/**
	 * @brief Create a sink collecting the catalog defaults of one owner.
	 * @param owner_id The parser/extractor identifier, used in error messages.
	 */
	explicit CatalogSink(std::string owner_id);

	/**
	 * @brief Build the set @p T through a recording source and record every
	 * (key, built-in literal) pair it declares.
	 *
	 * Each literal is STRICTLY compiled through the real engine, so a broken
	 * built-in fails the dump loudly. The set type selects the channel by its
	 * create() signature: `create(const html::SelectorSource&)` records into
	 * @ref selectors, `create(const RegexSource&)` into @ref patterns.
	 * @tparam T A selector or pattern set type.
	 * @throws std::logic_error when a recorded key collides with one already
	 *         recorded for this owner (a cross-set key collision), or when the
	 *         set's built-in literal fails to compile.
	 */
	template <class T>
	void emplace_set() {
		try {
			if constexpr (requires(const html::SelectorSource& source) { T::create(source); }) {
				RecordingSelectorSource source;
				[[maybe_unused]] const T set = T::create(source);
				record_all(selectors_, source.recorded, "selectors");
			} else if constexpr (requires(const RegexSource& source) { T::create(source); }) {
				RecordingRegexSource source;
				[[maybe_unused]] const T set = T::create(source);
				record_all(patterns_, source.recorded, "patterns");
			} else {
				static_assert(detail::always_false<T>,
				              "T::create must take a const html::SelectorSource& "
				              "or a const RegexSource&");
			}
		} catch (const std::exception& error) {
			throw std::logic_error(
			    fmt::format("catalog defaults of '{}': {}", owner_id_, error.what()));
		}
	}

	/**
	 * @brief Record the owner's canonical base URL (its public frontend origin).
	 * There is deliberately NO default (e.g. first mirror): an owner that does
	 * not call this has no canonical_base_url channel on the wire.
	 * @param url The canonical base URL.
	 * @throws std::logic_error on a second call (one owner, one canonical URL).
	 */
	void set_canonical_base_url(std::string url);

	/// @return The recorded selector key -> literal table (empty if none declared).
	[[nodiscard]] const RecordedSet& selectors() const noexcept { return selectors_; }
	/// @return The recorded pattern key -> literal table (empty if none declared).
	[[nodiscard]] const RecordedSet& patterns() const noexcept { return patterns_; }
	/// @return The canonical base URL, or nullopt when none was declared.
	[[nodiscard]] const std::optional<std::string>& canonical_base_url() const noexcept {
		return canonical_base_url_;
	}
	/// @return The owner identifier the sink was created for.
	[[nodiscard]] std::string_view owner_id() const noexcept { return owner_id_; }

private:
	/// Merge one set's recorded pairs into a channel; a repeated key is a
	/// cross-set collision and fails loudly.
	void record_all(RecordedSet& into, const RecordedSet& recorded, std::string_view channel);

	std::string owner_id_;
	RecordedSet selectors_;
	RecordedSet patterns_;
	std::optional<std::string> canonical_base_url_;
};

} // namespace aniparse
