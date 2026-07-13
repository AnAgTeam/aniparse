/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Model.hpp"

#include <optional>
#include <string>

namespace aniparse {

/**
 * @brief Display metadata for a parser — everything a UI needs to render it in a
 * source list, none of it load-bearing. Distinct from @ref Parser::identifier (the stable
 * routing/config key) and from @ref ParserCompatibilities (capability flags): this
 * is the витрина, returned by value so an enumeration over ParserStore::parsers()
 * can build a source picker with one call per parser and no capability payload.
 *
 * Icon/banner reuse @ref Image (the one media model bridged outward) so the app
 * has a uniform fetch descriptor; for a statically bundled parser the url is an
 * asset reference and headers are empty. Both are optional — a parser may ship no
 * artwork.
 */
struct ParserInfo {
	/// Human-facing display name (e.g. "AniList"). May differ from @ref Parser::identifier.
	std::string name;
	/// Primary content language as a BCP-47 tag (e.g. "en", "ru-ru"); "multi" or
	/// empty when the source is not tied to one language.
	std::string primary_language;
	/// Square logo / favicon, when the source has one.
	std::optional<Image> icon;
	/// Wide hero/preview artwork, when the source has one.
	std::optional<Image> banner;
};

} // namespace aniparse
