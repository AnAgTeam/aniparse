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
 * source actually knows:
 *
 * - the work exists and here is its title — @c title, nothing else;
 * - and this parser can open it — @c handle, ready for the matching root getter's
 *   from_serialized() (set only for a medium this parser serves);
 * - and here is where to find it elsewhere — @c external_ids, fed into a source that
 *   serves @c kind (@see search_key_for);
 * - or it knows a destination link — @c destination_url, resolved through the
 *   consumer's URL router with the destination catalog's own request profile.
 *
 * These are not exclusive: a source may hand out both a handle and ids, leaving
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
	/// Set when THIS parser can open the work directly: pass it to a compatible
	/// root getter of @c kind (from_serialized), possibly in another catalog of the
	/// same parser. Never construct or pass a handle belonging to another parser;
	/// use destination_url or external_ids for cross-parser references instead.
	std::optional<SerializedGetterData> handle;
	/// Absolute HTTP(S) title URL when the source knows a link to the related work.
	/// Consumers may route it to another parser/catalog; never fetch it using the
	/// originating work's profile or derive a foreign serialized handle from it.
	/// This is an address for routing, not a guarantee of a current canonical domain.
	/// A registered legacy domain is sufficient while the receiving parser accepts
	/// the URL format. The receiver resolves the identity and uses its own current
	/// endpoint settings; the originating parser need not obtain its canonical_url.
	/// A changed URL format requires compatibility in the receiver; replacing only
	/// the domain with a canonical one does not restore that compatibility.
	std::optional<std::string> destination_url;
};

} // namespace aniparse
