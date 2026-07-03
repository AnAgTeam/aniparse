/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Ids.hpp"

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace aniparse {
/// Minimal age allowed to access the item. Use 0 for all
using AgeRestriction = int;

struct ImageResolution {
	int width;
	int height;
};

struct Image {
	ImageID id{0};
	std::string url;
	std::optional<ImageResolution> size;
};

struct Tag {
	TagID id{0};
	std::string name;
	std::string referer;
};

struct RelatedUser {
	std::string name;
	std::string referer;
};

inline constexpr std::string_view series_original = "original";

struct Series {
	std::string name;
	std::string referer;
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

inline constexpr std::chrono::system_clock::time_point unknown_time{ std::chrono::system_clock::duration{0} };

/**
 * Structure representing rating (score) of the item (release, manga, etc.)
 */
struct Rating {
	/// Release rating is from 0 to 10. If the rating is from 0 to 5, just multiple it by 2 (3/5 -> 6/10)
	double total_rating = 0;
	/// Total sum of the 'rating' field. Used to calculate overall rating
	int total_raters = 0;
	/// Maximum rate in the 'rating' field, cannot be more than 10. Often sets to 2/5/10
	int max_rating = 0;
	/// Count for each of the rates positions. Undefined if max_rating == 0
	std::array<int, 10> rating = {};
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
} // namespace aniparse
