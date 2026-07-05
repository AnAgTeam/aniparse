/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Parser.hpp"
#include "aniparse/Exceptions.hpp"

#include <algorithm>

namespace aniparse {

namespace {

// Copy the map entries whose key is named in `names` (case-insensitivity follows
// the map's own comparator, so header names match case-insensitively).
template <typename Map>
Map pick_named(const Map& source, const std::vector<std::string>& names) {
	Map picked;
	for (const std::string& name : names) {
		if (auto it = source.find(name); it != source.end()) {
			picked.emplace(it->first, it->second);
		}
	}
	return picked;
}

std::vector<Cookie> pick_named_cookies(const std::vector<Cookie>& cookies,
                                       const std::vector<std::string>& names) {
	std::vector<Cookie> picked;
	for (const Cookie& cookie : cookies) {
		if (std::find(names.begin(), names.end(), cookie.name) != names.end()) {
			picked.push_back(cookie);
		}
	}
	return picked;
}

} // namespace

NetworkRequestTask<std::shared_ptr<const ParserConfig>> Parser::authenticate_context(
    RequestorContext, AuthenticationData) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot auth");
}

AuthKeys Parser::auth_keys() const noexcept {
	return {};
}

AuthState Parser::export_auth(const ParserConfig& config) const {
	AuthKeys keys = auth_keys();

	AuthState state;
	if (config.cookie_jar) {
		state.cookies = pick_named_cookies(config.cookie_jar->cookies(), keys.cookies);
	}
	state.headers    = pick_named(config.headers, keys.headers);
	state.url_params = pick_named(config.url_params, keys.url_params);
	state.alt_link   = config.alt_link;
	return state;
}

GetterSuggestionType Parser::suggest_getter(const ParsedUrl&) const {
	return GetterSuggestionType::Unknown;
}

bool Parser::valid_for_url(const ParsedUrl& url) const {
	// A URL is ours when we can route it to a getter category.
	return suggest_getter(url) != GetterSuggestionType::Unknown;
}

std::unique_ptr<ImagesGetter> Parser::images_getter() const {
	return nullptr;
}

std::unique_ptr<MangaRootGetter> Parser::mangas_getter() const {
	return nullptr;
}
} // namespace aniparse