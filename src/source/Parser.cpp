/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/source/Parser.hpp"
#include "aniparse/catalog/CatalogRootGetter.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace aniparse {

std::optional<std::string> Parser::suggest_catalog(const ParsedUrl&) const {
	return std::nullopt;
}

bool Parser::valid_for_url(const ParsedUrl& url) const {
	// A URL is ours when we can route it to a getter category.
	const auto suggestion = suggest_catalog(url);
	return suggestion.has_value();
}

std::unique_ptr<ImagesGetter> Parser::make_images_getter() const {
	return nullptr;
}

std::vector<ProfileInfo> Parser::profile_descriptors() const {
	return {{std::string(default_profile_id), "Default"}};
}

std::vector<ProfileInfo> Parser::profiles() const {
	auto available = profile_descriptors();
	if (available.empty()) {
		throw std::logic_error("A parser must declare at least one request profile");
	}
	std::set<std::string_view> ids;
	for (const auto& descriptor : available) {
		if (descriptor.id.empty() || !ids.insert(descriptor.id).second) {
			throw std::logic_error("Profile IDs must be nonempty and unique within a parser");
		}
	}
	return available;
}

std::unique_ptr<ParserProfile> Parser::make_profile(std::string_view profile_id) const {
	const auto available = profiles();
	const auto selected = std::find_if(available.begin(), available.end(),
		[profile_id](const auto& value) { return value.id == profile_id; });
	if (selected == available.end()) return nullptr;
	auto result = make_profile_impl(*selected);
	if (!result || result->info().id != selected->id) {
		throw std::logic_error("Profile factory returned a mismatched profile");
	}
	result->parser_id_ = identifier();
	return result;
}

std::unique_ptr<ParserProfile> Parser::make_profile_impl(ProfileInfo info) const {
	return std::make_unique<ParserProfile>(std::move(info));
}

std::vector<CatalogInfo> Parser::catalog_descriptors() const { return {}; }

std::vector<CatalogInfo> Parser::catalogs() const {
	auto available = catalog_descriptors();
	const auto declared_profiles = profiles();
	std::set<std::string_view> identifiers;
	for (const auto& catalog : available) {
		if (catalog.id.empty() || !identifiers.insert(catalog.id).second) {
			throw std::logic_error("Catalog IDs must be nonempty and unique within a provider");
		}
		if (std::none_of(declared_profiles.begin(), declared_profiles.end(),
				[&catalog](const auto& profile) { return profile.id == catalog.profile_id; })) {
			throw std::logic_error("Catalog references an unknown profile");
		}
	}
	return available;
}

ParserCapabilities Parser::capabilities() const {
	ParserCapabilities summary;
	for (const auto& catalog : catalogs()) {
		summary.flags |= catalog.capabilities.flags;
		// Preserve the old store summary for legacy source-list consumers.
		// Reading/watching are catalog metadata and have no global flag bits.
		if (catalog.kinds.has(catalog_kinds::manga)) summary.flags |= capabilities_flags::supports_manga_store;
		if (catalog.kinds.has(catalog_kinds::anime)) summary.flags |= capabilities_flags::supports_anime_store;
	}
	return summary;
}

std::unique_ptr<CatalogRootGetter> Parser::make_catalog_getter(std::string_view catalog_id) const {
	auto available = catalogs();
	const auto selected = std::find_if(available.begin(), available.end(),
		[catalog_id](const CatalogInfo& catalog) { return catalog.id == catalog_id; });
	if (selected == available.end()) return nullptr;
	auto root = make_catalog_getter_impl(*selected);
	if (!root || root->info().id != selected->id || root->info().profile_id != selected->profile_id) {
		throw std::logic_error("Catalog factory returned a root bound to another catalog");
	}
	return root;
}

std::unique_ptr<CatalogRootGetter> Parser::make_catalog_getter_impl(CatalogInfo) const { return nullptr; }

} // namespace aniparse
