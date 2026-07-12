/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Headers.hpp"
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
