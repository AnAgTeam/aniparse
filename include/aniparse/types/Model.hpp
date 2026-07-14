/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Headers.hpp"
#include "aniparse/types/Ids.hpp"
#include "aniparse/types/Text.hpp"

#include <span>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file
 * The pieces every domain's model is built from: an image to fetch, a tag, a person,
 * a series, a score, a publication state, an id on another site.
 *
 * These types are shared deliberately — a tag means the same thing whether it hangs
 * off a manga, an anime or an image container, so a consumer learns them once. Two
 * conventions run through the file and are worth reading before the individual
 * fields: an absent value (an empty string, a nullopt, @ref aniparse::unknown_time)
 * always means "the source did not state it", never "the item does not have it"; and
 * anything named @c ref is an opaque parser-owned handle, to be stored and passed
 * back, never parsed.
 */

namespace aniparse {
/// Minimal age allowed to access the item. Use 0 for all
using AgeRestriction = int;

/**
 * @brief Pixel dimensions of an image, as the source reports them.
 * Useful to size a placeholder before the bytes arrive; it is the source's claim
 * about the file, not a measurement of the decoded image.
 */
struct ImageResolution {
	int width;  ///< Width in pixels. Uninitialized unless set — always fill both.
	int height; ///< Height in pixels. Uninitialized unless set — always fill both.
};

/**
 * @brief Everything needed to fetch one image: the URL, the headers it may
 * require, and the dimensions when the source states them.
 *
 * The library models a picture as a fetch descriptor rather than bytes: nothing
 * here is downloaded, the consumer performs the request itself. Used for cover
 * art and thumbnails (the @c previews lists) as well as for content pages.
 */
struct Image {
	/// Source-local numeric handle, when the producing parser has one it wants to
	/// carry alongside the URL; 0 = none, and that is the usual case. It is not an
	/// identity a consumer can address anything by and it is not stable across
	/// sources — the URL is what identifies the image.
	ImageID id{0};
	/// Absolute URL the image is fetched from.
	std::string url;
	/// Pixel dimensions, when the source reports them. nullopt = the source does not
	/// state them (many listing endpoints omit dimensions for thumbnails); it never
	/// means the image is empty, and the dimensions can only be learned by decoding
	/// the fetched bytes.
	std::optional<ImageResolution> size;
	/// Extra request headers the consumer must send when fetching @ref Image::url — e.g.
	/// a Referer that some sources require to serve their images (a bare GET 403s
	/// otherwise). Empty when the plain URL suffices; set by the producing parser.
	Headers headers;
};

/**
 * @brief One label an item carries: a genre, a theme, a character, an artist —
 * whatever axis the source tags by, flattened into a single list.
 *
 * The list is the source's own vocabulary; the library does not normalize it, so
 * two parsers may spell the same concept differently. What makes a tag more than a
 * string is @ref Tag::ref — where the source can search by tags, the ref is the token that
 * searches for this tag, so a tag read off an item can be fed straight back into a
 * query.
 */
struct Tag {
	/// Source-local numeric handle for the tag, when the producing parser carries
	/// one; 0 = none, which is the usual case. Not the tag's identity for search —
	/// @ref Tag::ref is. Never assume it is comparable across sources.
	TagID id{0};
	/// Human-readable label, in whatever wording and language the source uses.
	/// Display-only: two sources may name the same concept differently, and the
	/// same tag may read differently on the same source over time.
	std::string name;
	/// Opaque parser-owned handle for this tag. NOT an HTTP Referer and not for
	/// the consumer to interpret — it is moved, never parsed. It round-trips to
	/// build follow-up requests; and where the parser supports searching by tag,
	/// it is the tag's search token: the same value advertised as the option's
	/// ItemSelection map key (search_keys::tag) and accepted back in search(),
	/// so a Tag from MangaInfo drops straight into a query.
	/// Prefer the source's stable identity (a slug/id) over a request path or
	/// URL, so a route or mirror change does not invalidate a stored handle; the
	/// parser rebuilds the request from it.
	/// Empty = this tag is not addressable on the source: it is a label to show,
	/// but nothing can be requested with it (a source that displays an axis it
	/// cannot filter by leaves the ref empty). Only a non-empty ref may be used as
	/// a search token.
	std::string ref;
};

/**
 * @brief A person or account credited on an item: an author, an artist, an
 * uploader, a translation team, the writer of a comment.
 *
 * A person is not modelled as a full entity — the library carries the name to show
 * plus an opaque handle to ask the source for more, and nothing else.
 */
struct RelatedUser {
	/// Display name as the source writes it. Empty = the source credits nobody in
	/// this role (an anonymous upload, an unattributed work) — it is not a promise
	/// that the role is inapplicable.
	std::string name;
	/// Opaque parser-owned reference; @see Tag::ref. Empty = the source exposes no
	/// handle for this person, so only the name is available.
	std::string ref;
};

/**
 * The conventional @ref aniparse::Series::name / @ref aniparse::Series::ref for a work that belongs to
 * no franchise — an original creation rather than a derivative of something else.
 * Sources that tag by franchise commonly use this exact token for that case, so
 * compare against this constant instead of treating it as a real series.
 */
inline constexpr std::string_view series_original = "original";

/**
 * @brief The franchise or parent work an item belongs to (its "copyright" on a
 * tag-based source, its parent series elsewhere) — the axis a consumer groups
 * derivative items by.
 */
struct Series {
	/// Display name of the series. @see series_original for the no-franchise case.
	std::string name;
	/// Opaque parser-owned reference; @see Tag::ref. Where the source can search by
	/// series it is that search token (@see search_keys::series); empty = the series
	/// is a label only and cannot be searched for here.
	std::string ref;
};

/**
 * @brief Which of a site's catalogues an ExternalId addresses.
 *
 * A site that catalogues both numbers them in SEPARATE id spaces: the same work
 * is one id as a manga and an unrelated id as its anime (on MyAnimeList, Frieren
 * is manga 126287 and anime 52991). A consumer holding both halves of one work
 * would collide them without this. Sites with a single catalogue still state it —
 * an AniDB id is an anime id.
 */
enum class MediaKind {
	Anime, ///< The id addresses the site's anime catalogue.
	Manga, ///< The id addresses the site's manga (or other printed-work) catalogue.
};

/**
 * @brief An identifier this item carries on another site, qualified by the
 * vocabulary and catalogue that own it — e.g.
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
	 * never taken apart — the catalogue is @ref kind, not a suffix to be parsed off.
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
	 * Two ids match only when they name the same identifier in the same catalogue of
	 * the same vocabulary — which is exactly the join a consumer performs to decide
	 * that two items from two parsers are one work.
	 */
	friend bool operator==(const ExternalId&, const ExternalId&) = default;
};

/**
 * @see AiredStatus::make_default()
 * Default supported AiredStatuses.
 * Used to create generic AiredStatus.
 */
enum class DefaultAiredStatuses {
	Released,  ///< Finished: everything the work will have is out. @see aired_status_released
	Ongoing,   ///< Publishing/airing now, more to come (a hiatus still reads as ongoing). @see aired_status_ongoing
	Announced, ///< Known but nothing released yet. @see aired_status_announced
	/**
	 * Anything the well-known set does not cover — a source-specific state
	 * (cancelled, licensed, on hold), or no state reported at all. A consumer that
	 * needs the distinction reads @ref AiredStatus::name.
	 */
	Other,
};

/// Canonical @ref AiredStatus::name for @ref aniparse::DefaultAiredStatuses::Released.
inline constexpr std::string_view aired_status_released  = "released";
/// Canonical @ref AiredStatus::name for @ref aniparse::DefaultAiredStatuses::Ongoing.
inline constexpr std::string_view aired_status_ongoing   = "ongoing";
/// Canonical @ref AiredStatus::name for @ref aniparse::DefaultAiredStatuses::Announced.
inline constexpr std::string_view aired_status_announced = "announced";

/**
 * @brief Where an item stands in its publication/airing life: released, ongoing,
 * announced, or something only that source names.
 *
 * The state is carried as a string rather than an enum because the set is open —
 * a source may have states the library never anticipated, and folding them into
 * an "other" bucket at parse time would throw the information away. A consumer
 * that only cares about the common cases calls @ref to_enum; one that wants to
 * show the source's own wording reads @ref name.
 */
struct AiredStatus {
	/// The state, as one of the canonical tokens (@ref aniparse::aired_status_released,
	/// @ref aniparse::aired_status_ongoing, @ref aniparse::aired_status_announced) when it maps onto
	/// one, otherwise the source's own wording. Empty = the source reported no
	/// status; that reads as @ref aniparse::DefaultAiredStatuses::Other, so an item whose
	/// status is unknown is not silently reported as released.
	std::string name;
	/// The moment the source attaches to that state, when it gives one. Equal to
	/// @ref aniparse::unknown_time (the default) when the source states only the state and no
	/// date — the common case, so treat a date here as a bonus and never as a
	/// reliable ordering key. Not to be confused with the item's own release_time.
	std::chrono::system_clock::time_point time;

	/**
	 * @brief Classify @ref name into the well-known set.
	 * @return The matching @ref aniparse::DefaultAiredStatuses, or DefaultAiredStatuses::Other
	 *         for a source-specific or empty name. Pure string match against the
	 *         canonical tokens — no locale or case folding.
	 */
	DefaultAiredStatuses to_enum() const;
};

/**
 * The value every time_point in the model carries when the source states no time:
 * the system_clock epoch, not a real timestamp. Every unset date field (an item's
 * release_time, a chapter's update_time, @ref aniparse::AiredStatus::time) compares equal to
 * this, so "unknown" is testable rather than merely early. A consumer must check for
 * it before formatting a date, or it will show 1970.
 */
inline constexpr std::chrono::system_clock::time_point unknown_time{std::chrono::system_clock::duration{0}};

/**
 * Structure representing rating (score) of the item (release, manga, etc.)
 *
 * Sources score on incompatible scales (5 stars, 0-10, a 0-100 mean), so the value
 * is stored normalized: @ref score is always on a 0-10 scale, whatever the source
 * used, and a consumer can compare or render it without knowing where it came from.
 * @ref max_score keeps the source's own scale for callers that want to show the raw
 * "8/10" or "83/100" form.
 */
class Rating {
public:
	/**
	 * @brief Construct rating from general information
	 * @param score Score [1, max]
	 * @param max_score Maximum score value
	 * @param raters Optionally, total count of votes
	 * @return rating
	 */
	static Rating from_score(double score, int max_score, std::optional<int> raters = std::nullopt) noexcept;

	/**
	 * @brief Construct rating from distrubution.
	 *        Other values i.e. 'score' is calculated from it.
	 * @param distribution Array of vote counts starting from 1, where distribution[i] = i vote count
	 * @throw Logic error if distribution.size() > 10
	 * @return rating
	 */
	static Rating from_distribution(std::span<const int> distribution);

	/**
	 * @brief Copy. A rating is a small value type, held by the item that carries it.
	 * @param other Rating to copy.
	 */
	Rating(const Rating& other) noexcept = default;
	/**
	 * @brief Move. Equivalent to a copy — the payload is trivially copyable.
	 * @param other Rating to move from; left valid and unchanged.
	 */
	Rating(Rating&& other) noexcept      = default;
	~Rating() noexcept                   = default;

	/**
	 * @brief Copy-assign.
	 * @param other Rating to copy.
	 * @return This rating.
	 */
	Rating& operator=(const Rating& other) noexcept = default;
	/**
	 * @brief Move-assign.
	 * @param other Rating to move from; left valid and unchanged.
	 * @return This rating.
	 */
	Rating& operator=(Rating&& other) noexcept      = default;

	/**
	 * @brief The score, rescaled to a common 0-10 axis whatever the source's own
	 * scale was, so ratings from different sources are directly comparable.
	 * @return The normalized score in [0, 10]. Multiply back by
	 *         @ref max_score / 10 to recover the value the source published.
	 */
	double score() const noexcept;

	/**
	 * @brief The source's own scale, kept only so the raw form can be shown.
	 * @return The maximum the source scores out of — as it publishes it (5, 10, 100),
	 *         not rescaled. This is NOT the range of @ref score.
	 */
	int max_score() const noexcept;

	/**
	 * @brief How many votes the score averages, when the source publishes it.
	 * @return The vote count, or nullopt when the source reports a score without a
	 *         vote count (so a lone 10/10 cannot be told from a well-established one).
	 */
	std::optional<int> raters() const noexcept;

	/**
	 * @brief The per-bucket vote breakdown, for sources that publish a histogram.
	 * @return Vote counts indexed from score 1 upward (element i = votes for score
	 *         i+1), spanning @ref max_score buckets; nullopt when the rating came
	 *         from a plain average, which is the common case. The span borrows from
	 *         this Rating and dies with it — copy it out to keep it.
	 */
	std::optional<std::span<const int>> distribution() const noexcept;

private:
	Rating() noexcept = default;

	double score_              = 0;
	int max_score_             = 0;
	std::optional<int> raters_ = 0;
	std::optional<std::array<int, 10>> distribution_;
};

/**
 * @see UserList::make_default()
 * Default supported UserLists.
 * Used to create generic UserList.
 */
enum class DefaultUserLists {
	Planning, ///< Queued to read/watch later. @see user_list_planning
	Dropped,  ///< Abandoned before finishing. @see user_list_dropped
	Favorite, ///< Marked a favourite; orthogonal to progress on some sources. @see user_list_favorite
	/// A list the well-known set does not name (e.g. "on hold", or a user-made
	/// list). The source's own wording stays in @ref UserList::name.
	Other,
	// video related
	Watched,  ///< Finished watching. @see user_list_watched
	Watching, ///< Currently watching. @see user_list_watching
	// book related
	Reading,  ///< Currently reading. @see user_list_reading
	Read,     ///< Finished reading. @see user_list_read
};

/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Planning.
inline constexpr std::string_view user_list_planning = "planning";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Dropped.
inline constexpr std::string_view user_list_dropped  = "dropped";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Favorite.
inline constexpr std::string_view user_list_favorite = "favorite";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Watched.
inline constexpr std::string_view user_list_watched  = "watched";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Watching.
inline constexpr std::string_view user_list_watching = "watching";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Reading.
inline constexpr std::string_view user_list_reading  = "reading";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Read.
inline constexpr std::string_view user_list_read     = "read";

/**
 * Structure representing list, that users added
 * the item. Usually in release, manga headers
 *
 * The list belongs to the account the request was made with, so it is meaningful
 * only on an authenticated source; an anonymous fetch cannot know it. Like
 * @ref AiredStatus the name is an open string, because a source may keep lists the
 * library does not name.
 */
struct UserList {
	/// The list, as one of the canonical tokens (@ref aniparse::user_list_reading, …) when it
	/// maps onto one, otherwise the source's own wording. Empty reads as
	/// @ref aniparse::DefaultUserLists::Other.
	std::string name;

	/**
	 * @brief Classify @ref name into the well-known set.
	 * @return The matching @ref aniparse::DefaultUserLists, or DefaultUserLists::Other for a
	 *         source-specific or empty name.
	 */
	DefaultUserLists to_enum() const;
};

/**
 * @brief How much attention an item has had on its source.
 * Its own struct rather than a bare int so an absent count (an optional ViewStats
 * left empty) is distinguishable from a real zero, and so further counters can be
 * added without breaking the field's users.
 */
struct ViewStats {
	/// Views the source reports, in whatever way it counts them — a source-defined
	/// number that is comparable between items of ONE source and meaningless across
	/// sources. 0 = genuinely none reported; an item whose source publishes no view
	/// count carries no ViewStats at all instead.
	int views = 0;
};

/**
 * @brief A user comment on an item (manga / anime / …). Available when the parser
 * advertises compatibilities_flags::supports_commenting.
 *
 * A single flat comment, or the root of a thread via @ref replies. Top-level
 * comments paginate through the getter's GetFilters like any other list; deep
 * reply pagination, when a source has it, is a later follow-up keyed by @ref ref.
 */
struct Comment {
	/// Who wrote it. Its name is empty for an anonymous post; its ref, when the
	/// source exposes one, addresses the account. @see RelatedUser
	RelatedUser author;
	/// The body, with any links the source marked up preserved as attributes.
	AttributedText text;
	/// When it was posted; @ref aniparse::unknown_time when the source does not date it
	/// (some report only a relative "2 days ago" the parser will not guess from).
	std::chrono::system_clock::time_point time = unknown_time;
	/// Net score / likes, if the source exposes one. Source-defined and possibly
	/// negative where downvotes exist. nullopt = the source has no voting on
	/// comments, or does not report it — never "zero votes".
	std::optional<int> score;
	/// Inline thread replies; empty when the source is flat or the comment has none.
	std::vector<Comment> replies;
	/// Opaque parser-owned handle; @see Tag::ref. Round-trips to fetch this
	/// comment's replies when a source paginates threads separately.
	std::string ref;
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
