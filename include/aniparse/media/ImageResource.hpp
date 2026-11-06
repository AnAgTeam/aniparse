/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/media/Image.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <variant>

namespace aniparse {

/**
 * @brief An encoded image compiled into the library, with no fetch step.
 *
 * Both views borrow immutable static storage. The bytes and MIME string must
 * remain valid for the lifetime of the loaded library; copying this descriptor
 * does not copy the image or extend the library's lifetime. Bytes contain the
 * original encoded file, not decoded pixels.
 */
struct EmbeddedImage {
	std::span<const std::byte> bytes; ///< Encoded image in static storage.
	std::string_view mime_type; ///< Media type, for example "image/png".
};

/// An image obtained through a fetch descriptor or directly from static bytes.
using ImageResource = std::variant<Image, EmbeddedImage>;

} // namespace aniparse
