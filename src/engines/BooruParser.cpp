/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/engines/BooruParser.hpp"
#include "aniparse/engines/BooruEngine.hpp"
#include "aniparse/engines/BooruImagesGetter.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"

#include <memory>
#include <string>
#include <variant>

namespace aniparse::engines {

ParserInfo BooruParser::info() const {
	return ParserInfo{
	    .name             = std::string(site_.name),
	    .primary_language = std::string(site_.primary_language),
	};
}

std::string BooruParser::identifier() const {
	return std::string(site_.identifier);
}

GetterSuggestionType BooruParser::suggest_getter(const ParsedUrl& url) const {
	// The engine knows the site's URL scheme; a post or (where supported) a pool URL
	// is an image container routed to the images getter.
	const BooruEngine& engine = site_.engine();
	const bool is_container = engine.post_id_from_url(url).has_value()
	                       || (engine.supports_pools() && engine.pool_id_from_url(url).has_value());
	return is_container ? GetterSuggestionType::Images : GetterSuggestionType::Unknown;
}

ParserCompatibilities BooruParser::compatibilities() const {
	// supports_images_store is derived, not declared: every booru parser hands out an
	// images getter (see images_getter), so the bit follows from the engine rather
	// than from a descriptor that can forget it.
	return { .flags = site_.compatibilities | compatibilities_flags::supports_images_store };
}

void BooruParser::emplace_domains(EmplaceDomainsContext& context) const {
	for (std::string_view domain : site_.domains) {
		context.add_domain(domain);
	}
}

void BooruParser::configure(ParserConfig& config) const {
	for (const auto& [name, value] : site_.engine().api_headers()) {
		config.headers.set(name, value);
	}
}

std::span<const std::string_view> BooruParser::mirrors() const {
	return site_.api_hosts;
}

std::unique_ptr<ImagesGetter> BooruParser::images_getter() const {
	return std::make_unique<BooruImagesGetter>(site_);
}

NetworkRequestTask<std::shared_ptr<const ParserConfig>> BooruParser::authenticate_context(
    RequestorContext context, AuthenticationData data) {
	if (!site_.auth) {
		// An anonymous source has nothing to authenticate.
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot auth");
	}

	const auto* creds = std::get_if<AuthenticationUserPassword>(&data);
	if (!creds) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		    "booru credentials must be user id (username) + api key (password)");
	}

	// Static query-param credentials: copy the base config and stamp them in. The
	// merged url_params ride on every request (ClientContext applies config params).
	auto config = std::make_shared<ParserConfig>(*context.config());
	config->url_params[std::string(site_.auth->user_param)] = creds->username;
	config->url_params[std::string(site_.auth->key_param)]  = creds->password;
	co_return std::shared_ptr<const ParserConfig>(std::move(config));
}

AuthKeys BooruParser::auth_keys() const noexcept {
	AuthKeys keys;
	if (site_.auth) {
		// The api key + user id url params are the durable credential to persist.
		keys.url_params = { std::string(site_.auth->key_param), std::string(site_.auth->user_param) };
	}
	return keys;
}

} // namespace aniparse::engines
