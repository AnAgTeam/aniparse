/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/media/Image.hpp"
#include "aniparse/media/Tags.hpp"
#include "aniparse/media/Identity.hpp"
#include "aniparse/media/Publication.hpp"
#include "aniparse/media/Rating.hpp"
#include "aniparse/media/Community.hpp"
#include <algorithm>
#include <utility>

namespace aniparse {

/**
 * @brief Another name for a work, optionally labelled by the source.
 * Labels are display text (a language, abbreviation, or other source wording),
 * not machine-readable language codes.
 */
struct AlternativeTitle {
	std::string title; ///< Non-empty alternative name.
	std::optional<std::string> label; ///< Source label, or nullopt when unstated.
};

/**
 * @brief The metadata every domain's item shares — the fields a manga, an anime and
 * an image container all carry — factored into one struct so a change to any of them
 * is made in one place, not three.
 *
 * Each domain's Info (@ref aniparse::MangaInfo, @ref aniparse::AnimeInfo,
 * @ref aniparse::ImageContainerInfo) embeds this as its @c common member and adds
 * its own typed id plus a short domain-specific tail (a manga's author/chapters, an
 * anime's season/episodes, a container's item count). Composition, not inheritance:
 * the Info types stay aggregates a parser fills with designated initializers, and the
 * Swift bridge imports them the same way.
 *
 * A few members here have no meaning in every domain — an image container states no
 * @ref external_ids, @ref canonical_title or @ref status. A domain that does not carry
 * one simply leaves it defaulted (empty / @c nullopt); the field's absence reads,
 * as everywhere else in the model, as "the source did not state it".
 */
struct MediaInfo {
	/// Ids this item carries on other sites, as the source reports them — the
	/// consumer's handle for joining the same work across parsers, including across
	/// domains (an anime and its manga are one work to a tracker). Empty when the
	/// source knows none (most reader sites; every image source). Not this parser's
	/// own identity: to address the item here, use the getter's serialize().
	/// @see ExternalId
	std::vector<ExternalId> external_ids;

	/// The title to show, in whichever language the source leads with (a source that
	/// carries several picks one; there is no promise it is English or romanized).
	/// Empty only when the source does not name the item at all — some domains (a
	/// tag-only image catalog) always synthesize a stable non-empty title instead.
	std::string title;
	/// The source's commonly recognized name for matching across sources, often
	/// romanized for Japanese works. May equal title. nullopt = the source does not
	/// provide one; consumers must not treat a name as a unique work identity.
	std::optional<std::string> canonical_title;
	/// Other names, including native-script names, translations and abbreviations.
	/// Preserve source order; omit empty names, duplicates, title and canonical_title.
	std::vector<AlternativeTitle> alternative_titles;
	/// Synopsis, plain text plus any links the source marked up. Empty = the source
	/// gives none here (usual for a listing card, and some works simply have none).
	/// Not HTML: markup is flattened into the text or lifted into the attributes.
	/// @see AttributedText
	AttributedText description;

	/// When the item was last touched on the source (new chapter/episode, an edit).
	/// @c nullopt = the source does not state it. Sources differ on what counts as an
	/// update, so this orders items within one source only.
	std::optional<ModelDate> update_time;
	/// When the item first became available to read, watch, or otherwise consume.
	/// @c nullopt = the source does not state an actual availability date, including
	/// when it only announces a future premiere. A planned installment belongs in
	/// the domain-specific upcoming-installment field instead. @see ModelDate for
	/// coarse precisions.
	std::optional<ModelDate> release_time;
	/// Publication/airing state (ongoing / released / announced / source-specific). A
	/// default-constructed status (empty name) = the source states none, and reads as
	/// DefaultAiredStatuses::Other rather than as "released". Left default by domains
	/// with no such axis (image containers). @see AiredStatus
	AiredStatus status;
	/// Opaque change marker for the whole item, filled from the cheapest signal the
	/// source exposes (an ETag, an updated-at value, an explicit version, a
	/// composite). Compared only for equality: a changed value means the source
	/// reports the content as a different revision — a cheap "probably unchanged"
	/// hint, not a content-integrity guarantee. Empty = the source exposes no such
	/// signal.
	std::string revision;

	/// The franchise(s)/parent work(s) the source places the item in, primary first.
	/// Empty = it places the item in none — the work stands alone or the source has no
	/// such axis. Usually one, but a source that tags by franchise (a doujin's
	/// parodies, a booru's copyrights) can list several for one work.
	std::vector<Series> series;
	/// The account that posted the item, on sources where content is user-submitted.
	/// @c nullopt = the source has no notion of an uploader (a publisher-side catalog).
	std::optional<RelatedUser> uploader;

	/// Cover art and thumbnails, best first (a consumer showing one shows previews
	/// front()). Empty = the source offers no artwork; the images are fetch
	/// descriptors, not bytes. @see Image
	std::vector<Image> previews;
	/// The source's labels for this item — genres, themes, whatever axes it tags by,
	/// flattened into one list. A tag whose @ref Tag::ref is non-empty can be fed back
	/// into a search; empty = the source lists none in this response.
	std::vector<Tag> tags;

	/// Community score, normalized to a 0-10 axis (@see Rating). @c nullopt = the
	/// source publishes no score for this item, which is not the same as a score of
	/// zero.
	std::optional<Rating> rating;
	/// View/popularity counters. @c nullopt = the source publishes none. @see ViewStats
	std::optional<ViewStats> views;
	/// Public per-list audience counts, such as users planning, watching, or having
	/// completed the work. Empty = the source publishes no such breakdown in this
	/// response. This is aggregate data, distinct from the authenticated user's
	/// @ref UserList membership.
	std::vector<AudienceListCount> audience_lists;
	/// A recurring release cadence the source explicitly declares. @c nullopt =
	/// no cadence is published in this response; consumers must not derive one
	/// from previous installments. @see RecurringReleaseInfo
	std::optional<RecurringReleaseInfo> release_schedule;

	/// Minimum age the source requires to view the item, in years. 0 = unrestricted or
	/// unstated — an adult work is more reliably detected via @ref is_hentai and the
	/// source's own adult flag. @see AgeRestriction
	AgeRestriction age_restriction = 0;
	/// The source marks this item as adult/pornographic. False = it does not mark it,
	/// which is a weaker statement than "safe": sources differ on where the line sits,
	/// and one that has no adult flag at all leaves this false throughout.
	bool is_hentai = false;

	/**
	 * @brief Append a distinct alternative name in source order.
	 * @param name Candidate name; empty, display, canonical and repeated names are skipped.
	 * @param label Optional source-provided display label.
	 */
	void add_alternative_title(std::string name, std::optional<std::string> label = std::nullopt) {
		if (name.empty() || name == title || (canonical_title && name == *canonical_title)) return;
		if (std::ranges::any_of(alternative_titles, [&](const AlternativeTitle& value) {
			return value.title == name;
		})) return;
		if (label && label->empty()) label.reset();
		alternative_titles.push_back({ .title = std::move(name), .label = std::move(label) });
	}

};

} // namespace aniparse
