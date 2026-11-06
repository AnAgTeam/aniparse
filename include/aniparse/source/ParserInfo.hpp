/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/media/ImageResource.hpp"

#include <optional>
#include <string>

namespace aniparse {

/**
 * @brief Display metadata for a parser — everything a UI needs to render it in a
 * source list, none of it load-bearing. Distinct from @ref Parser::identifier (the stable
 * routing/config key) and from @ref ParserCapabilities (capability flags): this
 * is the витрина, returned by value so an enumeration over ParserStore::parsers()
 * can build a source picker with one call per parser and no capability payload.
 *
 * The optional banner uses @ref ImageResource — either an Image fetch descriptor
 * or an EmbeddedImage borrowing compiled-in bytes. Catalog logos belong to
 * CatalogInfo::icon, so independently selectable catalogs can have distinct
 * artwork. Reading this metadata never fetches or decodes an image.
 */
struct ParserInfo {
	/// Human-facing display name (e.g. "AniList"). May differ from @ref Parser::identifier.
	std::string name;
	/// Primary content language as a BCP-47 tag (e.g. "en", "ru-ru"); "multi" or
	/// empty when the source is not tied to one language.
	std::string primary_language;
	/// Wide hero/preview artwork, when the source has one.
	std::optional<ImageResource> banner;
};

} // namespace aniparse
