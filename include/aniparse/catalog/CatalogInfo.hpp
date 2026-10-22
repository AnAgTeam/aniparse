/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/source/Capabilities.hpp"
#include "aniparse/source/ProfileInfo.hpp"

#include <string>

namespace aniparse {

/**
 * @brief A strongly typed 64-bit mask describing a catalog's possible formats.
 *
 * This is UI metadata, not a filter or a promise about every returned card.
 * Bit positions are local to this mask and unrelated to CapabilitiesFlags.
 */
using CatalogKinds = FlagsBitfield<64, struct CatalogKindsTag>;

namespace catalog_kinds {
/// Bit 0 denotes catalogues that can return manga getters.
inline constexpr auto manga = CatalogKinds::make_bit(0);
/// Bit 1 denotes catalogues that can return anime getters.
inline constexpr auto anime = CatalogKinds::make_bit(1);
} // namespace catalog_kinds

/**
 * @brief Summary capabilities for a source landing page or catalog picker.
 *
 * These declarations describe the catalog as a whole. Reading or watching may
 * be available only for some works; callers still handle item-level errors.
 * They never disable a getter operation or trigger speculative network requests.
 */
struct CatalogCapabilities {
	bool reading = false; ///< At least part of the catalog offers chapter pages.
	bool watching = false; ///< At least part of the catalog offers video sources.
	CapabilitiesFlags flags; ///< Other summary metadata, such as adult content.
};

/**
 * @brief One independently selectable catalog supplied by a parser.
 *
 * IDs are stable and unique within a parser; multiple scopes may advertise the
 * same kinds. Persist the scope ID alongside the parser ID, format and item
 * identity. A mixed scope declares both bits without requiring both in each card.
 * The selected root owns a copy, so it remains valid after this vector is gone.
 */
struct CatalogInfo {
	std::string id; ///< Opaque stable selection key, not a content-type name.
	std::string name; ///< Human-readable catalog name.
	CatalogKinds kinds; ///< Formats this catalog may return; UI metadata only.
	CatalogCapabilities capabilities; ///< Capabilities of this particular scope.
	std::string profile_id{default_profile_id}; ///< Request profile selected through Parser::profile.
};

} // namespace aniparse
