/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aniparse {

/**
 * @brief A read-time view over a parser's mirror (base-URL) list: a remote
 * override if one exists, otherwise the getter's built-in fallback.
 *
 * Used internally by operation snapshots and by extractors. The override pointer aliases the
 * context's held MirrorSource snapshot and the built-in span aliases the getter's
 * static list, so a returned base_url() view stays valid for the context's
 * lifetime (a whole operation) without copying.
 */
class Mirrors {
public:
	/**
	 * @param override_list The remote override list, or nullptr for none.
	 * @param builtin The getter's built-in fallback base URLs.
	 */
	Mirrors(const std::vector<std::string>* override_list,
	        std::span<const std::string_view> builtin) noexcept
	    : override_(override_list), builtin_(builtin) {}

	/// @return How many mirrors are actually available (the override list when it
	/// is present and non-empty, else the built-in count) — the true size to bound
	/// a selection against, kept consistent with base_url()'s fall-through.
	[[nodiscard]] std::size_t count() const noexcept {
		return override_ && !override_->empty() ? override_->size() : builtin_.size();
	}

	/**
	 * @brief The base URL for a mirror selection, preferring the override list.
	 * An out-of-range index clamps to the first entry. Uses the built-in fallback
	 * when there is no (non-empty) override.
	 * @param index Position in this view; session selection stores the URL itself.
	 * @return The base URL, or an empty view if no mirror is available at all.
	 */
	[[nodiscard]] std::string_view base_url(std::size_t index) const noexcept {
		if (override_ && !override_->empty()) {
			return (*override_)[index < override_->size() ? index : 0];
		}
		if (builtin_.empty()) {
			return {};
		}
		return builtin_[index < builtin_.size() ? index : 0];
	}

private:
	const std::vector<std::string>* override_;
	std::span<const std::string_view> builtin_;
};

/**
 * @brief Remote mirror overrides, keyed by parser identifier.
 *
 * Fed straight from SourceOverrides::mirrors. An entry replaces a parser's built-in
 * base URLs at runtime (dead frontend host swapped for a live one) without an app
 * rebuild; a parser with no entry keeps its built-ins. An EMPTY source reproduces
 * every parser's built-ins exactly.
 */
class MirrorSource {
public:
	/**
	 * @brief Override keys identify a parser and one of its request profiles.
	 */
	using ProfileKey = std::pair<std::string, std::string>;
	/// Transparent ordering avoids allocating temporary ID strings during snapshots.
    struct ProfileKeyLess {
        using is_transparent = void;
        template<class Left, class Right>
        bool operator()(const Left& left, const Right& right) const {
            return std::pair<std::string_view, std::string_view>{left.first, left.second} <
                   std::pair<std::string_view, std::string_view>{right.first, right.second};
        }
    };
    using ProfileMirrors = std::map<ProfileKey, std::vector<std::string>, ProfileKeyLess>;
    using ProfileOrigins = std::map<ProfileKey, std::string, ProfileKeyLess>;

	MirrorSource() = default;

	/**
	 * @brief Build extractor mirrors and profile-specific request origins.
	 * @param overrides Mirror lists for explicit-ID consumers such as extractors.
	 * @param profile_mirrors Overrides keyed by parser ID and profile ID.
	 * @param profile_origins Frontend origins keyed by parser ID and profile ID.
	 */
	explicit MirrorSource(
	    std::map<std::string, std::vector<std::string>, std::less<>> overrides,
		ProfileMirrors profile_mirrors = {}, ProfileOrigins profile_origins = {})
		: overrides_(std::move(overrides)),
		  profile_mirrors_(std::move(profile_mirrors)), profile_origins_(std::move(profile_origins)) {}

	/**
	 * @brief The override list for @p parser_id, if present.
	 * @return A pointer to the ordered base URLs, or nullptr when the parser has
	 *         no override (the caller then uses its built-in fallback). Valid while
	 *         this source lives.
	 */
	[[nodiscard]] const std::vector<std::string>* list_for(std::string_view parser_id) const {
		auto it = overrides_.find(parser_id);
		return it == overrides_.end() ? nullptr : &it->second;
	}

	/**
	 * @brief Resolve a profile's mirror override without crossing profile boundaries.
	 * @param parser_id Stable owning parser identifier.
	 * @param profile_id Stable profile identifier bound by Parser::make_profile.
	 * @return A list owned by this source, or nullptr for built-in fallback.
	 */
	const std::vector<std::string>* list_for(
		std::string_view parser_id, std::string_view profile_id) const {
		auto it = profile_mirrors_.find(std::pair<std::string_view, std::string_view>{parser_id, profile_id});
		if (it != profile_mirrors_.end()) return &it->second;
		return nullptr;
	}

	/**
	 * @brief Resolve a profile's canonical origin independently of other profiles.
	 * @param parser_id Stable owning parser identifier.
	 * @param profile_id Stable profile identifier bound by Parser::make_profile.
	 * @return An origin owned by this source, or nullptr for built-in fallback.
	 */
	const std::string* canonical_base_for(
		std::string_view parser_id, std::string_view profile_id) const {
		auto it = profile_origins_.find(std::pair<std::string_view, std::string_view>{parser_id, profile_id});
		if (it != profile_origins_.end()) return &it->second;
		return nullptr;
	}

	/// @return true if the source carries no overrides (every parser falls back).
	[[nodiscard]] bool empty_source() const noexcept {
		return overrides_.empty()
			&& profile_mirrors_.empty() && profile_origins_.empty();
	}

private:
	std::map<std::string, std::vector<std::string>, std::less<>> overrides_;
	ProfileMirrors profile_mirrors_;
	ProfileOrigins profile_origins_;
};

} // namespace aniparse
