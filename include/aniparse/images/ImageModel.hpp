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

/**
 * @file
 * The images data model: a container and the media items inside it — the values an
 * ImageContainerGetter returns.
 *
 * "Container" is the one abstraction the domain needs: a source whose unit is a
 * single tagged post and a source whose unit is a gallery are the same shape, one
 * holding a single item and the other many. The items are not necessarily still
 * pictures, so each declares its own @ref aniparse::ImageItemKind. Absent values
 * follow the model-wide rule: an empty string, a nullopt or
 * @c nullopt means the source did not state it.
 */

namespace aniparse {

/**
 * The source's own numeric id for a container. Meaningful only within the one source
 * that issued it — never compare ids across parsers, and never treat it as this
 * library's handle on the container (that is the getter).
 * @see ImageContainerInfo::id
 */
using ImageContainerID = int;
/// The ImageContainerID that addresses nothing: what @ref aniparse::ImageContainerInfo::id
/// carries when the source has no numeric id for the container. Also the value a
/// listing entry is rejected on, since a container that cannot be addressed cannot
/// be fetched.
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
	/// What @ref image actually points at, so a consumer picks a decoder/player
	/// without sniffing the URL. Defaults to Still: a source that also serves video
	/// must set it. The URL's extension is a hint, not the contract — this field is.
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
	/// The source's own id for the container (its post or gallery number).
	/// @ref aniparse::invalid_image_container_id (0) = the source has no numeric id for it.
	ImageContainerID id = invalid_image_container_id;

	/// The metadata shared with every other domain — title, description, dates, tags,
	/// series, previews, rating, and the rest. An image container leaves the members
	/// that have no meaning for it (@ref MediaInfo::external_ids,
	/// @ref MediaInfo::original_title, @ref MediaInfo::status) defaulted. Note that
	/// @ref MediaInfo::title is never empty here — a tag-only catalog whose posts have
	/// no name gets a stable synthesized title (built from its most telling tags, or
	/// its id), which is not necessarily something the source itself would display and
	/// is not an identity. @see aniparse::MediaInfo
	MediaInfo common;

	/// How many ImageItems the container holds, when the source states it up
	/// front (a booru post is 1); absent when only discoverable by paging items().
	std::optional<long> total_items;
};

} // namespace aniparse
