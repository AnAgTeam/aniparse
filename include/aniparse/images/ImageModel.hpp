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
 * The images DATA model, split out of Image.hpp so it can be included without the
 * getter interfaces — and therefore without coroutines, expected, or the client.
 * Image.hpp includes this and adds the getters, so nothing else changes.
 *
 * Kept transport-free on purpose: this is the half a consumer binds to (the Swift
 * bridge imports it directly), and a header that reaches NetworkRequestTask cannot
 * be imported by Swift's clang importer at all. @see MangaModel.hpp
 */
namespace aniparse {

using ImageContainerID = int;
inline constexpr ImageContainerID invalid_image_container_id = ImageContainerID{ 0 };

struct ImageContainerGetter;

/**
 * @brief What kind of media one item in a container is. A booru-style source
 * mixes stills and video in the same tagged catalog, so the leaf declares its
 * own kind instead of assuming "still". @see ImageItem
 */
enum class ImageItemKind {
	Still,    ///< A single raster image (jpg/png/webp).
	Animated, ///< An animated image with no audio track (gif/animated webp).
	Video,    ///< A video clip with its own container/codec (webm/mp4).
};

/**
 * @brief One media element of a container: the fetchable resource plus what it
 * is. The leaf of the images domain, mirroring MangaPage. A flat single-media
 * source (a booru post) yields one; a gallery yields many.
 */
struct ImageItem {
	/// The fetch descriptor — url + headers + resolution. @see Image
	Image image;
	ImageItemKind kind = ImageItemKind::Still;
	/// Playback length for Animated/Video; absent for stills or when unknown.
	std::optional<std::chrono::milliseconds> duration;
	/// A poster/thumbnail distinct from the media itself, when the source offers
	/// one (typically a video's still frame); empty otherwise.
	std::optional<Image> poster;
};

/**
 * @brief Metadata of one container — a single booru post or a whole gallery.
 * The images counterpart of MangaInfo: a plain data model returned by
 * ImageContainerGetter::info, not a bag of getter methods.
 */
struct ImageContainerInfo {
	ImageContainerID id = invalid_image_container_id;

	std::string title;
	AttributedText description;
	std::vector<Tag> tags;

	std::optional<RelatedUser> uploader;
	std::optional<Series> series;

	/// Cover/sample images for the container as a whole (grid thumbnails).
	std::vector<Image> previews;

	std::optional<Rating> rating;
	std::optional<ViewStats> views;

	std::chrono::system_clock::time_point update_time  = unknown_time;
	std::chrono::system_clock::time_point release_time = unknown_time;

	/// Opaque change marker for the whole container; @see MangaInfo::revision.
	std::string revision;

	AgeRestriction age_restriction = 0;

	/// How many ImageItems the container holds, when the source states it up
	/// front (a booru post is 1); absent when only discoverable by paging items().
	std::optional<long> total_items;

	bool is_hentai = false;
};

} // namespace aniparse
