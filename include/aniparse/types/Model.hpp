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

namespace aniparse {
/// Minimal age allowed to access the item. Use 0 for all
using AgeRestriction = int;

struct ImageResolution {
	int width;
	int height;
};

struct Image {
	ImageID id{0};
	/// Absolute URL the image is fetched from.
	std::string url;
	/// Pixel dimensions, when the source reports them.
	std::optional<ImageResolution> size;
	/// Extra request headers the consumer must send when fetching @ref Image::url — e.g.
	/// a Referer that some sources require to serve their images (a bare GET 403s
	/// otherwise). Empty when the plain URL suffices; set by the producing parser.
	Headers headers;
};

struct Tag {
	TagID id{0};
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
	std::string ref;
};

struct RelatedUser {
	std::string name;
	/// Opaque parser-owned reference; @see Tag::ref.
	std::string ref;
};

inline constexpr std::string_view series_original = "original";

struct Series {
	std::string name;
	/// Opaque parser-owned reference; @see Tag::ref.
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
	Anime,
	Manga,
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
 * (@see search_keys::mal_id), not here.
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

	friend bool operator==(const ExternalId&, const ExternalId&) = default;
};

/**
 * @see AiredStatus::make_default()
 * Default supported AiredStatuses.
 * Used to create generic AiredStatus.
 */
enum class DefaultAiredStatuses {
	Released,
	Ongoing,
	Announced,
	Other,
};

inline constexpr std::string_view aired_status_released  = "released";
inline constexpr std::string_view aired_status_ongoing   = "ongoing";
inline constexpr std::string_view aired_status_announced = "announced";

struct AiredStatus {
	std::string name;
	std::chrono::system_clock::time_point time;

	DefaultAiredStatuses to_enum() const;
};

inline constexpr std::chrono::system_clock::time_point unknown_time{std::chrono::system_clock::duration{0}};

/**
 * Structure representing rating (score) of the item (release, manga, etc.)
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

	Rating(const Rating& other) noexcept = default;
	Rating(Rating&& other) noexcept      = default;
	~Rating() noexcept                   = default;

	Rating& operator=(const Rating& other) noexcept = default;
	Rating& operator=(Rating&& other) noexcept      = default;

	/**
	 * @return Normalized score [1, max]
	 */
	double score() const noexcept;

	/**
	 * @return max score value [1, 10]
	 */
	int max_score() const noexcept;

	/**
	 * @return Total raters count (votes count)
	 */
	std::optional<int> raters() const noexcept;

	/**
	 * @return Optional distribution for rating
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
	Planning,
	Dropped,
	Favorite,
	Other,
	// video related
	Watched,
	Watching,
	// book related
	Reading,
	Read,
};

inline constexpr std::string_view user_list_planning = "planning";
inline constexpr std::string_view user_list_dropped  = "dropped";
inline constexpr std::string_view user_list_favorite = "favorite";
inline constexpr std::string_view user_list_watched  = "watched";
inline constexpr std::string_view user_list_watching = "watching";
inline constexpr std::string_view user_list_reading  = "reading";
inline constexpr std::string_view user_list_read     = "read";

/**
 * Structure representing list, that users added
 * the item. Usually in release, manga headers
 */
struct UserList {
	std::string name;

	DefaultUserLists to_enum() const;
};

struct ViewStats {
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
	RelatedUser author;
	AttributedText text;
	std::chrono::system_clock::time_point time = unknown_time;
	/// Net score / likes, if the source exposes one.
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
inline constexpr std::string_view anilist      = "anilist";
inline constexpr std::string_view kitsu        = "kitsu";
inline constexpr std::string_view shikimori    = "shikimori";
/// Anime only — an id here is always MediaKind::Anime.
inline constexpr std::string_view anidb        = "anidb";
/// Manga only — an id here is always MediaKind::Manga.
inline constexpr std::string_view mangaupdates = "mangaupdates";
} // namespace aniparse::id_namespaces
