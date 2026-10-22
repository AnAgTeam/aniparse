/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/source/ParserProfile.hpp"
#include <algorithm>
#include <stdexcept>

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

NetworkRequestTask<std::shared_ptr<const ParserConfig>> ParserProfile::authenticate(
	RequestContext, AuthenticationData) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot auth");
}

ParserProfile::ParserProfile(std::string parser_id, ProfileInfo info)
	: parser_id_(std::move(parser_id)), info_(std::move(info)) {
	if (parser_id_.empty() || info_.id.empty()) {
		throw std::invalid_argument("Parser and profile IDs must be nonempty");
	}
}

std::shared_ptr<ParserConfig> ParserProfile::make_config() const {
	auto config = std::make_shared<ParserConfig>();
	config->parser_id = parser_id_;
	config->profile_id = info_.id;
	configure(*config);
	return config;
}

void ParserProfile::configure(ParserConfig&) const {}

std::span<const std::string_view> ParserProfile::mirrors() const {
	return {};
}

std::string ParserProfile::canonical_base_url() const {
	return {};
}

std::vector<AltLink> ParserProfile::mirror_choices(const RequestContext& context) const {
	return resolve_alt_links(mirrors(), context);
}

std::optional<AuthInfo> ParserProfile::auth_info() const {
	return std::nullopt;
}

std::optional<AuthInfo> ParserProfile::auth_info(RequestContext) const {
	return auth_info();
}

AuthKeys ParserProfile::auth_keys() const noexcept {
	// Derive from the declared credential shape so a parser states it once in
	// auth_info(); a parser with a credential a field list cannot name (e.g. a
	// session cookie from a username/password login) overrides this directly.
	if (std::optional<AuthInfo> info = auth_info()) {
		return auth_keys_from_fields(info->fields);
	}
	return {};
}

AuthState ParserProfile::export_auth(const ParserConfig& config) const {
	AuthKeys keys = auth_keys();

	AuthState state;
	if (config.cookie_jar) {
		state.cookies = pick_named_cookies(config.cookie_jar->cookies(), keys.cookies);
	}
	for (const std::string& name : keys.headers) {
		for (std::string_view value : config.headers.get_all(name)) {
			state.headers.append(name, std::string(value));
		}
	}
	state.url_params = pick_named(config.url_params, keys.url_params);
	state.alt_link   = config.alt_link;
	return state;
}


} // namespace aniparse
