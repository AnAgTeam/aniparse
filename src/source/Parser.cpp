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

GetterSuggestionResult Parser::suggest_getter(const ParsedUrl&) const {
	return GetterSuggestionResult{ .type = GetterSuggestionType::Unknown };
}

bool Parser::valid_for_url(const ParsedUrl& url) const {
	// A URL is ours when we can route it to a getter category.
	const auto suggestion = suggest_getter(url);
	return suggestion.scope_id.has_value() || suggestion.type != GetterSuggestionType::Unknown;
}

std::unique_ptr<ImagesGetter> Parser::images_getter() const {
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

std::unique_ptr<ParserProfile> Parser::profile(std::string_view profile_id) const {
	const auto available = profiles();
	const auto selected = std::find_if(available.begin(), available.end(),
		[profile_id](const auto& value) { return value.id == profile_id; });
	if (selected == available.end()) return nullptr;
	auto result = make_profile(*selected);
	if (!result || result->info().id != selected->id || result->parser_id() != identifier()) {
		throw std::logic_error("Profile factory returned a mismatched profile");
	}
	return result;
}

std::unique_ptr<ParserProfile> Parser::make_profile(ProfileInfo info) const {
	return std::make_unique<ParserProfile>(identifier(), std::move(info));
}

std::vector<CatalogInfo> Parser::scope_descriptors() const { return {}; }

std::vector<CatalogInfo> Parser::scopes() const {
	auto available = scope_descriptors();
	const auto declared_profiles = profiles();
	std::set<std::string_view> identifiers;
	for (const auto& scope : available) {
		if (scope.id.empty() || !identifiers.insert(scope.id).second) {
			throw std::logic_error("Catalog scope IDs must be nonempty and unique within a provider");
		}
		if (std::none_of(declared_profiles.begin(), declared_profiles.end(),
				[&scope](const auto& profile) { return profile.id == scope.profile_id; })) {
			throw std::logic_error("Catalog references an unknown profile");
		}
	}
	return available;
}

ParserCapabilities Parser::capabilities() const {
	ParserCapabilities summary;
	for (const auto& scope : scopes()) {
		summary.flags |= scope.capabilities.flags;
		// Preserve the old store summary for legacy source-list consumers.
		// Reading/watching are catalog metadata and have no global flag bits.
		if (scope.kinds.has(catalog_kinds::manga)) summary.flags |= capabilities_flags::supports_manga_store;
		if (scope.kinds.has(catalog_kinds::anime)) summary.flags |= capabilities_flags::supports_anime_store;
	}
	return summary;
}

std::unique_ptr<CatalogRootGetter> Parser::catalog_getter(std::string_view scope_id) const {
	auto available = scopes();
	const auto selected = std::find_if(available.begin(), available.end(),
		[scope_id](const CatalogInfo& scope) { return scope.id == scope_id; });
	if (selected == available.end()) return nullptr;
	auto root = make_catalog_getter(*selected);
	if (!root || root->scope().id != selected->id || root->scope().profile_id != selected->profile_id) {
		throw std::logic_error("Catalog factory returned a root bound to another scope");
	}
	return root;
}

std::unique_ptr<CatalogRootGetter> Parser::make_catalog_getter(CatalogInfo) const { return nullptr; }

} // namespace aniparse
