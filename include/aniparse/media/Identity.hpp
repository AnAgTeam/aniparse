/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include <string>
#include <string_view>

namespace aniparse {

/**
 * @brief Which of a site's catalogues an ExternalId addresses.
 *
 * A site that catalogues both numbers them in SEPARATE id spaces: the same work
 * is one id as a manga and an unrelated id as its anime (on MyAnimeList, Frieren
 * is manga 126287 and anime 52991). A consumer holding both halves of one work
 * would collide them without this. Sites with a single catalog still state it —
 * an AniDB id is an anime id.
 */
enum class MediaKind {
	Anime, ///< The id addresses the site's anime catalog.
	Manga, ///< The id addresses the site's manga (or other printed-work) catalog.
};

/**
 * @brief An identifier this item carries on another site, qualified by the
 * vocabulary and catalog that own it — e.g.
 * { id_namespaces::mal, MediaKind::Anime, "52991" }.
 *
 * The consumer's cross-source anchor. A source emits whatever foreign ids it
 * happens to know (a metadata API returning its MyAnimeList id, a reader site
 * carrying the tracker id it was matched against); the consumer collects them
 * and joins items across parsers by them. This is the *emitting* side and it is
 * open-ended — a parser fills what it has and nothing else.
 *
 * Emission is not symmetric with lookup: an item announcing an external id says
 * nothing about the source being able to *find* an item by one. That is a
 * separate, rarely offered capability and it is declared as a search filter key
 * (@see search_keys::mal_id), not here. The asymmetry runs both ways — a source
 * MUST accept its own vocabulary as a lookup key, and MUST NOT emit it here.
 *
 * **A parser never emits its own namespace.** These ids say where else the item
 * lives, and a source restating that its own item is its own says nothing. The
 * item's own id is already in the model (@c id), and what reopens it later is the
 * getter's serialize() handle — the identity built to survive a restart. An
 * external id exists for the item a *different* parser would have to find.
 *
 * @note A namespace names an identity vocabulary, NOT a parser. The two are
 *       independent: a namespace may have no parser at all (a consumer keying on
 *       MyAnimeList ids need not have a MyAnimeList parser), and a source freely
 *       emits ids of namespaces it does not own. Never derive one from the other.
 */
struct ExternalId {
	/**
	 * The owning vocabulary. A well-known constant from @ref aniparse::id_namespaces
	 * where one fits; the field is a plain string and the vocabulary is open, so a
	 * parser may mint a namespace this header does not list. Opaque: matched whole,
	 * never taken apart — the catalog is @ref kind, not a suffix to be parsed off.
	 */
	std::string ns;
	/**
	 * Which of the site's catalogues @ref id addresses. Required: a work's anime id
	 * and its manga id are unrelated numbers in the same namespace. @see MediaKind
	 */
	MediaKind kind;
	/**
	 * The identifier as the owning site writes it, verbatim. Opaque: it is stored
	 * and compared, never parsed or reformatted (a site's ids may be numeric today
	 * and slugs tomorrow).
	 */
	std::string id;

	/**
	 * @brief Equality over all three fields, all compared verbatim.
	 * Two ids match only when they name the same identifier in the same catalog of
	 * the same vocabulary — which is exactly the join a consumer performs to decide
	 * that two items from two parsers are one work.
	 */
	friend bool operator==(const ExternalId&, const ExternalId&) = default;
};

} // namespace aniparse

/**
 * Well-known identity vocabularies for ExternalId::ns.
 *
 * A shared spelling so two parsers that both know an item's MyAnimeList id agree
 * on how to say so — without it the ids are unjoinable. The list is a convenience,
 * not a closed set: ExternalId::ns is a plain string and a parser may emit a
 * namespace absent here.
 *
 * These name identity vocabularies only. A namespace here implies no parser for
 * it, and a parser implies no namespace. @see ExternalId
 */
namespace aniparse::id_namespaces {
/// MyAnimeList. The de facto hub: most metadata sites carry a MAL id, which makes
/// it the practical bridge between two vocabularies that do not know each other
/// directly. @see search_keys::mal_id
inline constexpr std::string_view mal          = "mal";
/// A metadata site cataloguing both anime and manga — an id here always states its
/// @ref aniparse::MediaKind.
inline constexpr std::string_view anilist      = "anilist";
/// A metadata site cataloguing both anime and manga — an id here always states its
/// @ref aniparse::MediaKind.
inline constexpr std::string_view kitsu        = "kitsu";
/// A metadata site cataloguing both anime and manga — an id here always states its
/// @ref aniparse::MediaKind.
inline constexpr std::string_view shikimori    = "shikimori";
/// Anime only — an id here is always MediaKind::Anime.
inline constexpr std::string_view anidb        = "anidb";
/// Manga only — an id here is always MediaKind::Manga.
inline constexpr std::string_view mangaupdates = "mangaupdates";
} // namespace aniparse::id_namespaces
