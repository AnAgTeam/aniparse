/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/Headers.hpp"
#include "aniparse/media/Ids.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
 * @brief One size of an image, with its own URL and optional pixel dimensions.
 * Variants represent the same artwork and crop. Different artwork belongs in a
 * separate Image. Missing dimensions mean the source did not state them.
 */
struct ImageVariant {
	std::string url; ///< Absolute URL to fetch.
	std::optional<ImageResolution> size; ///< Source-reported dimensions, if known.
};

/**
 * @brief An image and its available size variants, sharing one fetch policy.
 * The first variant is the source's recommended default. A single known URL
 * becomes one variant; no variants means the source offers no image. Headers
 * and adapter apply to every variant. Descriptors contain no downloaded bytes.
 */
struct Image {
	/// Optional source-local numeric handle; 0 means absent. Not a cross-source ID.
	ImageID id{0};
	/// Available sizes of the same artwork, recommended default first.
	std::vector<ImageVariant> variants;
	/// Extra headers required when fetching any variant (for example Referer).
	Headers headers;
	/// Shared optional byte transformation policy; null means fetch directly.
	std::shared_ptr<const ResourceAdapter> adapter;
};

} // namespace aniparse
