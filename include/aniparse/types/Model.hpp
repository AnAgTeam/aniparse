/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Headers.hpp"
#include "aniparse/types/Ids.hpp"
#include "aniparse/types/Serialization.hpp" // RelatedWork carries an openable handle
#include "aniparse/types/Text.hpp"

#include <span>
#include <array>
#include <chrono>
#include <cstdint>
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
 * fields: an absent value (an empty string, a @c nullopt @ref aniparse::ModelDate)
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
	/// The categorical axis this tag belongs to: a @ref aniparse::search_keys value
	/// (@ref search_keys::tag, @ref search_keys::genre, @ref search_keys::character,
	/// @ref search_keys::type, @ref search_keys::language, ...). It names the filter
	/// GROUP; @ref ref is the token WITHIN it, so the pair (axis, ref) fully addresses
	/// this tag in a query — search() routes ref into the filter keyed by axis.
	/// Empty = @ref search_keys::tag, the default axis, so a tag that predates this
	/// field (or a source with a single, unnamed tag axis) behaves exactly as before.
	/// Where the source has no per-axis filter at all, ref carries the whole token and
	/// axis is a display-only grouping hint. A consumer may section its tag display by
	/// this value, and MUST degrade to a flat list when it is empty.
	std::string axis;
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
 * @see ExternalId, MangaGetter::related, similar
 */
struct RelatedWork {
	/// Which catalogue the related work lives in — the axis that makes this
	/// cross-media. Equal to the item's own medium for a sequel or a spin-off;
	/// different for an adaptation.
	MediaKind kind = MediaKind::Manga;
	/**
	 * How the source says the two are related, in the source's own word ("adaptation",
	 * "sequel", "side_story", "alternative", ...). Free text on purpose: every catalogue
	 * has its own relation vocabulary, and flattening them into an enum would either
	 * lose the distinctions a source draws or freeze one site's taxonomy into the model.
	 * Empty when the source states a relation without naming it.
	 */
	std::string relation;
	/// Display title of the related work.
	std::string title;
	/// Cover art, when the source hands it out with the relation.
	std::vector<Image> previews;
	/// The work's ids on sites that catalogue @c kind — how a consumer opens it on a
	/// source this parser has nothing to do with. Empty when the source names no ids.
	std::vector<ExternalId> external_ids;
	/// Set when THIS parser can open the work directly: pass it to the root getter of
	/// @c kind (from_serialized). Empty when the work lives in a medium this parser
	/// does not serve — the common case for an adaptation, and why @c external_ids
	/// exists.
	std::optional<SerializedGetterData> handle;
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
 * @brief How precisely a source dated something — the granularity of a @ref aniparse::ModelDate.
 *
 * A source may give a full calendar day, or only a coarser bucket: a month, a quarter
 * (an anime "season" — Winter/Spring/Summer/Fall map onto Q1..Q4), or a bare year. The
 * date's @ref aniparse::ModelDate::time is pinned to the FIRST instant of that bucket (a year →
 * Jan 1, Q3 → Jul 1), so ordering still works; this field says how much of it is real,
 * so a consumer renders "2025" / "Q1 2025" / "March 2025" / the full date instead of a
 * fabricated January 1.
 */
enum class DatePrecision : std::uint8_t {
	Day,     ///< A full calendar day is known (the default). Render the whole date.
	Month,   ///< Only the month is known — render e.g. "March 2025".
	Quarter, ///< Only the quarter / anime season — @ref aniparse::ModelDate::time is its first day.
	Year,    ///< Only the year is known — render e.g. "2025".
};

/**
 * @brief A moment a source attaches to an item, carrying how precisely it is known.
 *
 * Wrapped rather than a bare time_point so the precision cannot drift from the value.
 * A date field is @c std::optional<ModelDate>: @c nullopt means the source gave no date
 * at all — an absent date is absent, not a sentinel epoch — and a present value is
 * always a real moment plus its @ref aniparse::DatePrecision.
 */
struct ModelDate {
	/// The instant, pinned to the first moment of the precision bucket (@ref aniparse::DatePrecision).
	std::chrono::system_clock::time_point time;
	/// How much of @ref aniparse::ModelDate::time the source actually stated.
	DatePrecision precision = DatePrecision::Day;
};

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
	/// The moment the source attaches to that state, when it gives one. @c nullopt (the
	/// default) when the source states only the state and no date — the common case, so
	/// treat a date here as a bonus and never as a reliable ordering key. Not to be
	/// confused with the item's own release_time.
	std::optional<ModelDate> time;

	/**
	 * @brief Classify @ref name into the well-known set.
	 * @return The matching @ref aniparse::DefaultAiredStatuses, or DefaultAiredStatuses::Other
	 *         for a source-specific or empty name. Pure string match against the
	 *         canonical tokens — no locale or case folding.
	 */
	DefaultAiredStatuses to_enum() const;
};


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
	/// When it was posted; @c nullopt when the source does not date it
	/// (some report only a relative "2 days ago" the parser will not guess from).
	std::optional<ModelDate> time;
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
 * @ref external_ids, @ref original_title or @ref status. A domain that does not carry
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
	/// The title in the work's original language/script, when the source carries one
	/// AND it differs from @ref title. @c nullopt = no separate original title, or it
	/// is the same string — so a consumer never renders the title twice.
	std::optional<std::string> original_title;
	/// Synopsis, plain text plus any links the source marked up. Empty = the source
	/// gives none here (usual for a listing card, and some works simply have none).
	/// Not HTML: markup is flattened into the text or lifted into the attributes.
	/// @see AttributedText
	AttributedText description;

	/// When the item was last touched on the source (new chapter/episode, an edit).
	/// @c nullopt = the source does not state it. Sources differ on what counts as an
	/// update, so this orders items within one source only.
	std::optional<ModelDate> update_time;
	/// When the item was first published/aired. @c nullopt = the source does not state
	/// it — common when only a year is known. @see ModelDate for coarse precisions.
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

	/// Minimum age the source requires to view the item, in years. 0 = unrestricted or
	/// unstated — an adult work is more reliably detected via @ref is_hentai and the
	/// source's own adult flag. @see AgeRestriction
	AgeRestriction age_restriction = 0;
	/// The source marks this item as adult/pornographic. False = it does not mark it,
	/// which is a weaker statement than "safe": sources differ on where the line sits,
	/// and one that has no adult flag at all leaves this false throughout.
	bool is_hentai = false;
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
