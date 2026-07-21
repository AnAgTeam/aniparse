/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Ids.hpp"
#include "aniparse/types/Model.hpp"
#include "aniparse/types/Text.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

/*
 * The manga DATA model, split out of Manga.hpp so it can be included without the
 * getter interfaces — and therefore without coroutines, expected, or the client.
 * Manga.hpp includes this and adds the getters, so nothing else changes.
 *
 * Kept transport-free on purpose: this is the half a consumer binds to (the Swift
 * bridge imports it directly), and a header that reaches NetworkRequestTask cannot
 * be imported by Swift's clang importer at all.
 */

/**
 * @file
 * The manga data model: a manga, its translations, its chapters, its pages — the
 * values a MangaGetter returns and a consumer renders. No request hides behind any
 * field here; the getters do the fetching, these types only carry what came back.
 *
 * Two rules run through the file. An absent value (an empty string, a nullopt,
 * @c nullopt) means the source did not state it in that response,
 * never that the manga lacks it — the same struct is filled shallowly for a search
 * card and fully for a detail fetch. And identity is the opaque handle, not the
 * numbers: a chapter is addressed by MangaChapterInfo::ref(), not by its volume and
 * chapter number, which exist for display and grouping.
 */

namespace aniparse {

struct MangaGetter;

/**
 * The source's own numeric id for a manga. Meaningful only within the one source
 * that issued it — never compare ids across parsers (that is what ExternalId is
 * for), and never treat it as this library's handle on the manga (that is the
 * getter). @see MangaInfo::id
 */
using MangaID            = int;
/**
 * The source's own id for one translation of a manga, as advertised by
 * @ref aniparse::MangaTranslationInfo::id. Meaningful only within that source, and only
 * alongside the manga it was listed for; it is what a chapter listing or a page
 * fetch is narrowed by.
 */
using MangaTranslationID = int;

struct MangaGetter;

/**
 * @see MangaType
 * The well-known publication formats. Used to build a generic @ref aniparse::MangaType
 * without hardcoding its wording.
 */
enum class DefaultMangaType {
	Manga,    ///< Commercially published Japanese comic.
	Dojinshi, ///< Self-published/fan work.
	Manhwa,   ///< Korean comic.
	Manhua,   ///< Chinese comic.
	Other     ///< Anything the set above does not name; the wording stays in @ref aniparse::MangaType::name.
};

/**
 * @brief The publication format of a manga (manga, manhwa, doujinshi, …), carried
 * as an open string for the same reason @ref AiredStatus is: a source may publish
 * formats this library does not enumerate, and folding them into "other" at parse
 * time would lose them.
 */
struct MangaType {
	/**
	 * @brief Build the type from one of the well-known formats.
	 * @param type The format to name. Implicit, so a @ref aniparse::DefaultMangaType may be
	 *             passed wherever a MangaType is expected.
	 */
	MangaType(DefaultMangaType type);

	/// The format, in the source's own wording where it has one. Display-oriented:
	/// the library does not normalize it, so a consumer that needs to branch should
	/// match leniently rather than assume a spelling.
	std::string name;
};

/// The MangaID that addresses nothing: the value @ref aniparse::MangaInfo::id carries when
/// the source has no numeric id for the manga (it addresses works by slug, say).
/// It is not an error marker — the manga is still fully usable through its getter.
inline constexpr MangaID invalid_manga_id = MangaID{ 0 };

/**
 * @brief Everything a source states about one manga: what it is called, who made
 * it, how it is tagged, where it stands. The data half of MangaGetter — a plain
 * value the consumer holds and renders, with no request behind any field.
 *
 * The same struct serves two fetch depths. A listing (a search result, a browse
 * card) yields it filled with only what a cheap listing endpoint carries; a full
 * fetch fills what the detail endpoint carries. So an empty string or a nullopt
 * here means "this source did not state it in this response", not "the manga does
 * not have it": a value absent from a search card can appear once the manga is
 * fetched in full.
 */
struct MangaInfo {
	/// The source's own numeric id, when it has one. @ref aniparse::invalid_manga_id (0) = it
	/// does not — which is not an error. This is a display/debugging convenience and
	/// a source-local key; it is neither a cross-source identity (@ref external_ids)
	/// nor the handle to fetch with (@see MangaGetter::serialize).
	MangaID id = invalid_manga_id;

	/// Ids this manga carries on other sites, as the source reports them — the
	/// consumer's handle for joining the same work across parsers. Empty when the
	/// source knows none (most reader sites); a metadata source typically knows at
	/// least its MyAnimeList id. Not this parser's own identity: to address the
	/// manga here, use the getter (@see MangaGetter::serialize). @see ExternalId
	std::vector<ExternalId> external_ids;

	/// The title to show, in whichever language the source leads with (a source that
	/// carries several picks one; there is no promise it is English or romanized).
	/// Empty only when the source does not name the manga at all.
	std::string title;
	/// The title in the work's original language/script, when the source carries one
	/// AND it differs from @ref title. nullopt = the source has no separate original
	/// title, or it is the same string — so a consumer never renders the title twice.
	std::optional<std::string> original_title;
	/// Synopsis, plain text plus any links the source marked up. Empty = the source
	/// gives no description here (usual for a search card, and some works simply
	/// have none). Not HTML: markup is either flattened into the text or lifted into
	/// the attributes. @see AttributedText
	AttributedText description;

	/// When the manga was last touched on the source (a new chapter, an edit).
	/// @c nullopt = the source does not state it. Sources differ on what
	/// counts as an update, so this orders items within one source only.
	std::optional<ModelDate> update_time;
	/// When the manga was first published. @c nullopt = the source does not
	/// state it — common when only a year is known.
	std::optional<ModelDate> release_time;
	/// Publication state (ongoing / released / announced / source-specific). A
	/// default-constructed status (empty name) = the source states none, and reads as
	/// DefaultAiredStatuses::Other rather than as "released". @see AiredStatus
	AiredStatus status;

	/// Opaque change marker for the whole manga, filled from the cheapest
	/// signal the source exposes (an ETag, an updated-at value, an explicit
	/// version, or a composite). Compared only for equality: a changed value
	/// means the source reports the content as a different revision. Equality
	/// is a cheap "probably unchanged" hint, not a content-integrity guarantee
	/// (it does not catch a single re-uploaded chapter or mid-list id drift).
	/// Empty = the source exposes no such signal.
	std::string revision;

	/// Who wrote it. Its name is empty when the source credits no author (or does not
	/// carry the credit in this response); the same person is often credited as both
	/// author and artist, in which case both fields name them. @see RelatedUser
	RelatedUser author;
	/// Who drew it. Empty name = not credited here; see @ref author.
	RelatedUser artist;
	/// The franchise(s)/parent work(s) the source places the manga in, primary first.
	/// Empty = it places the manga in none — the manga stands alone or the source has
	/// no such axis. Usually one, but a source that tags by franchise (a doujin's
	/// parodies, a booru's copyrights) can list several for one work.
	std::vector<Series> series;

	/// Cover art and thumbnails, best first (a consumer showing one shows previews
	/// front()). Empty = the source offers no artwork; the images are fetch
	/// descriptors, not bytes. @see Image
	std::vector<Image> previews;
	/// The source's labels for this manga — genres, themes, whatever axes it tags by,
	/// flattened into one list. A tag whose @ref Tag::ref is non-empty can be fed
	/// back into a search; empty = the source lists none in this response.
	std::vector<Tag> tags;

	/// Community score, normalized to a 0-10 axis (@see Rating). nullopt = the source
	/// publishes no score for this manga, which is not the same as a score of zero.
	std::optional<Rating> rating;
	/// View/popularity counters. nullopt = the source publishes none. @see ViewStats
	std::optional<ViewStats> views;
	/// The list the authenticated user keeps this manga on (reading, planning, …).
	/// nullopt = the request was anonymous, the source has no lists, or the user has
	/// not filed this manga — the three are not distinguishable here. @see UserList
	std::optional<UserList> user_lists;
	/// Minimum age the source requires to view the manga, in years. 0 = unrestricted
	/// or unstated — an adult work is more reliably detected via @ref is_hentai and
	/// the source's own adult flag. @see AgeRestriction
	AgeRestriction age_restriction = 0;

	/// The account that posted the manga, on sources where content is user-submitted.
	/// nullopt = the source has no notion of an uploader (a publisher-side catalog).
	std::optional<RelatedUser> uploader;

	/// How many chapters the source claims the manga has, when it says so up front.
	/// nullopt = it does not, and the only way to know is to page chapters_info().
	/// The source's claim: a running series' count may lag its own chapter list, and
	/// it counts the work's chapters, not the chapters this source hosts.
	std::optional<long> total_chapters;

	/// How many pages the work has, when the source states it up front — meaningful for
	/// one-shots and galleries (a booru pool, a doujin) whose whole length is one number.
	/// nullopt = the source does not report it. Distinct from @ref aniparse::MangaInfo::total_chapters — a work
	/// is a count of chapters OR, when it has none, a count of pages.
	std::optional<long> total_pages;

	/// The source marks this manga as adult/pornographic. False = it does not mark
	/// it, which is a weaker statement than "safe": sources differ on where the line
	/// sits, and one that has no adult flag at all leaves this false throughout.
	bool is_hentai = false;
};

/**
 * @brief One translation of a manga: the axis a chapter list is selected on before
 * chapters exist. A source that hosts several scanlations of the same work exposes
 * one of these per translation, and chapters_info() / chapter_pages() are narrowed
 * by its @ref id. Sources with a single translation do not expose the axis at all
 * (their MangaGetter reports translation_info as NotImplemented).
 */
struct MangaTranslationInfo {
	/// The source's handle for this translation; what a chapter listing or a page
	/// fetch is narrowed by. Meaningful only within its source and its manga.
	MangaTranslationID id;
	/// The language this translation is in, as the source labels it — a tag such as
	/// "ru-ru" or a plain language name, not normalized by the library. Empty = the
	/// source does not label it (a single-language site often does not).
	std::string language;
	/// The group/person behind the translation. Its name is empty when the source
	/// credits none. @see RelatedUser
	RelatedUser translator;
};

/**
 * @brief Identity of one chapter: everything chapter_pages() needs and
 * nothing else, so the call stays cheap (typically zero heap: two longs
 * plus an empty/SSO string).
 * Obtained via MangaChapterInfo::ref() and round-tripped unchanged; for
 * simple numerically-addressed sources it can also be constructed
 * directly, e.g. MangaChapterRef{ .chapter = 12 }.
 */
struct MangaChapterRef {
	/// Volume the chapter sits in; 0 = the source does not use volumes (or does not
	/// state one). Load-bearing only when @ref id is empty.
	long volume  = 0;
	/// Chapter number within the volume. 0 = unnumbered. Load-bearing only when
	/// @ref id is empty; a fractional chapter cannot be expressed here, which is one
	/// reason a source that has them addresses chapters by @ref id instead.
	long chapter = 0;
	/// Opaque handle from the getter that produced the info.
	/// Empty = the getter identifies the chapter by the numeric fields
	std::string id;
};

/**
 * @brief Everything a chapter listing states about one chapter. The item type of
 * MangaGetter::chapters_info; its @ref ref() is what fetches the chapter's pages.
 *
 * A chapter's identity is the ref, never the numbers: the numeric fields are for
 * grouping and display, and several chapters may legitimately share them.
 */
struct MangaChapterInfo {
	/// Numeric hints for grouping/ordering in UI; not the chapter's identity.
	/// Sources with fractional or unnumbered chapters ("7.5", "Extra") cannot
	/// express them losslessly here — that is what number and id are for.
	/// Volume the chapter belongs to; 0 = the source does not volume its chapters.
	long volume  = 0;
	/// Chapter number, whole part only; 0 = unnumbered (an extra, a one-shot), so it
	/// is not a usable sort key on its own. @see number
	long chapter = 0;
	/// Chapter number exactly as the source spells it: "7.5", "Extra".
	/// Empty = render from volume/chapter
	std::string number;
	/// Opaque chapter handle, understood only by the getter that produced
	/// this info; round-trips into chapter_pages() via ref().
	std::string id;
	/// The chapter's own title, when it has one. Empty = untitled — which is the norm;
	/// it does not contain the chapter number, so a UI shows both.
	std::string name;
	/// Translator's note or blurb attached to the chapter. Empty = none, the usual case.
	std::string description;
	/// Thumbnails for the chapter (often its first page). Empty = the source offers
	/// none. Fetch descriptors, not the chapter's pages — those come from
	/// chapter_pages(). @see Image
	std::vector<Image> previews;
	/// When the chapter was last edited/re-uploaded on the source; @c nullopt
	/// = not stated.
	std::optional<ModelDate> update_time;
	/// When the chapter was published on the source; @c nullopt = not stated.
	/// This is the source's posting date, not the original serialization date.
	std::optional<ModelDate> release_time;

	/**
	 * @brief Identity for the chapter_pages() round-trip
	 * @return The ref built from this info's numbers and opaque id. Pass it back
	 *         unchanged — the numbers alone identify a chapter only on sources that
	 *         leave @ref id empty. Cheap: nothing is fetched, the id is copied.
	 */
	[[nodiscard]] MangaChapterRef ref() const { return { volume, chapter, id }; }
};

/**
 * @brief One page of a chapter: a single fetchable image. The leaf of the manga
 * domain and the item type of MangaGetter::chapter_pages, whose page order is the
 * reading order.
 *
 * A struct rather than a bare @ref Image so a source that carries more per page
 * (dimensions, a decryption hint) can grow the type without breaking callers.
 */
struct MangaPage {
	/// The page's fetch descriptor — URL, any headers the fetch requires, and the
	/// dimensions when the source states them. Its url is never empty for a page a
	/// source actually serves. @see Image
	Image image;
};

} // namespace aniparse
