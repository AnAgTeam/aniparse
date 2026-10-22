/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/Headers.hpp"
#include "aniparse/media/Ids.hpp"
#include <memory>
#include <optional>
#include <string>

namespace aniparse {

class ResourceAdapter;

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
	/// Optional parser-provided policy for preparing and transforming this image's
	/// bytes. Null when a consumer may fetch @ref url directly. The adapter is
	/// shared because the same immutable policy can serve previews, pages, and
	/// concurrent loads; the concrete type is forward-declared here so this data
	/// model does not import transport or coroutine declarations.
	std::shared_ptr<const ResourceAdapter> adapter;
};

} // namespace aniparse
