/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/media/Identity.hpp"
#include "aniparse/media/Image.hpp"
#include "aniparse/media/Serialization.hpp"
#include <optional>
#include <string>
#include <vector>

namespace aniparse {

/**
 * @brief One work the source itself declares as related to this item: another season,
 * a spin-off, a side story — or the same story in another medium (a manga's anime
 * adaptation, an anime's source manga).
 *
 * The cross-media axis of the model. A getter is per-medium (a MangaGetter has
 * chapters, an AnimeGetter has episodes), so a relation that leaves the medium cannot
 * be expressed as a getter of the same kind — and a source that catalogues manga
 * usually cannot open the anime it names, only name it. This descriptor states what a
 * source actually knows, which is one of three things, in rising order of usefulness:
 *
 * - the work exists and here is its title — @c title, nothing else;
 * - and this parser can open it — @c handle, ready for the matching root getter's
 *   from_serialized() (set only for a medium this parser serves);
 * - and here is where to find it elsewhere — @c external_ids, fed into a source that
 *   serves @c kind (@see search_key_for).
 *
 * The three are not exclusive: a source may hand out both a handle and ids, leaving
 * the consumer to choose between reopening the work here and following it out.
 *
 * @note Related work is NOT identity. Cross-media ids live here, never in
 *       ExternalId::external_ids of the item itself — a manga carrying its anime's id
 *       as its own would collide with the anime's real identity, and any consumer
 *       joining items by external id would merge two different works into one.
 * @see ExternalId, MangaGetter::load_related, similar
 */
struct RelatedWork {
	/// Which catalog the related work lives in — the axis that makes this
	/// cross-media. Equal to the item's own medium for a sequel or a spin-off;
	/// different for an adaptation.
	MediaKind kind = MediaKind::Manga;
	/**
	 * How the source says the two are related, in the source's own word ("adaptation",
	 * "sequel", "side_story", "alternative", ...). Free text on purpose: every catalog
	 * has its own relation vocabulary, and flattening them into an enum would either
	 * lose the distinctions a source draws or freeze one site's taxonomy into the model.
	 * Empty when the source states a relation without naming it.
	 */
	std::string relation;
	/// Display title of the related work.
	std::string title;
	/// Cover art, when the source hands it out with the relation.
	std::vector<Image> previews;
	/// The work's ids on sites that catalog @c kind — how a consumer opens it on a
	/// source this parser has nothing to do with. Empty when the source names no ids.
	std::vector<ExternalId> external_ids;
	/// Set when THIS parser can open the work directly: pass it to the root getter of
	/// @c kind (from_serialized). Empty when the work lives in a medium this parser
	/// does not serve — the common case for an adaptation, and why @c external_ids
	/// exists.
	std::optional<SerializedGetterData> handle;
};

} // namespace aniparse
