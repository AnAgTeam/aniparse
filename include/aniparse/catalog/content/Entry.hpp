/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/anime/Anime.hpp"
#include "aniparse/manga/Manga.hpp"

#include <concepts>

namespace aniparse {

/**
 * @brief One source-owned catalog card with one or both content formats.
 *
 * Owns its getters. A successful catalog operation never returns an empty entry.
 * Group formats only when the source itself groups them, never by matching names
 * or numeric IDs. Each getter retains its own identity and listing preview.
 */
struct CatalogEntry {
	std::unique_ptr<MangaGetter> manga; ///< Manga metadata and existing reading API.
	std::unique_ptr<AnimeGetter> anime; ///< Anime metadata and existing video API.
	std::optional<MediaKind> preferred_preview; ///< Source preference, if any.

	/**
	 * @brief Construct an empty draft.
	 *
	 * Populate at least one getter before returning it from an operation.
	 */
	CatalogEntry() = default;

	/**
	 * @brief Construct a single-format card without another preview copy.
	 * @tparam Getter Concrete manga or anime getter type.
	 * @param getter Getter whose ownership is transferred to the card.
	 */
	template<class Getter> requires (std::derived_from<Getter, MangaGetter> || std::derived_from<Getter, AnimeGetter>)
	CatalogEntry(std::unique_ptr<Getter> getter) {
		if constexpr (std::derived_from<Getter, MangaGetter>) manga = std::move(getter);
		else anime = std::move(getter);
	}

	/**
	 * @brief Select a free listing preview without requesting details.
	 * @param preferred Consumer preference, overriding the source hint.
	 * @return Common preview fields, or nullopt. Falls back to the other available
	 *         preview; without a preference the stable order is manga, then anime.
	 */
	[[nodiscard]] std::optional<MediaInfo> preview_info(std::optional<MediaKind> preferred = std::nullopt) const;

	/**
	 * @brief Check whether the card contains a getter of a particular format.
	 * @param kind Requested content format.
	 * @return Whether the corresponding getter is present.
	 */
	[[nodiscard]] bool has(MediaKind kind) const noexcept;

	/**
	 * @brief Check whether the card lacks both content getters.
	 * @return True only when neither getter is present.
	 */
	[[nodiscard]] bool empty() const noexcept { return !manga && !anime; }
};

} // namespace aniparse
