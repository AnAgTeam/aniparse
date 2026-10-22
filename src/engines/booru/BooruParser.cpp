/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/engines/booru/BooruParser.hpp"
#include "aniparse/engines/booru/BooruEngine.hpp"
#include "aniparse/engines/booru/BooruImagesGetter.hpp"
#include "aniparse/runtime/RequestContext.hpp"
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

GetterSuggestionResult BooruParser::suggest_getter(const ParsedUrl& url) const {
	// The engine knows the site's URL scheme; a post or (where supported) a pool URL
	// is an image container routed to the images getter.
	const BooruEngine& engine = site_.engine();
	const bool is_container = engine.post_id_from_url(url).has_value()
	                       || (engine.supports_pools() && engine.pool_id_from_url(url).has_value());
	return is_container ? GetterSuggestionResult{ .type = GetterSuggestionType::Images } : GetterSuggestionResult{ .type = GetterSuggestionType::Unknown };
}

ParserCapabilities BooruParser::capabilities() const {
	// supports_images_store is derived, not declared: every booru parser hands out an
	// images getter (see images_getter), so the bit follows from the engine rather
	// than from a descriptor that can forget it.
	return { .flags = site_.capabilities | capabilities_flags::supports_images_store };
}

void BooruParser::emplace_domains(EmplaceDomainsContext& context) const {
	for (std::string_view domain : site_.domains) {
		context.add_domain(domain);
	}
}


namespace {
class BooruProfile final : public ParserProfile {
public:
    BooruProfile(std::string parser_id, ProfileInfo info, const BooruSite& site)
        : ParserProfile(std::move(parser_id), std::move(info)), site_(site) {}
    void configure(ParserConfig& config) const override;
    std::span<const std::string_view> mirrors() const override;
    NetworkRequestTask<std::shared_ptr<const ParserConfig>> authenticate(
	    RequestContext context,
	    AuthenticationData data) const override;
    AuthKeys auth_keys() const noexcept override;
private:
    const BooruSite& site_;

};
} // namespace

void BooruProfile::configure(ParserConfig& config) const {
	for (const auto& [name, value] : site_.engine().api_headers()) {
		config.headers.set(name, value);
	}
}

std::span<const std::string_view> BooruProfile::mirrors() const {
	return site_.api_hosts;
}

std::unique_ptr<ImagesGetter> BooruParser::images_getter() const {
	return std::make_unique<BooruImagesGetter>(site_);
}

NetworkRequestTask<std::shared_ptr<const ParserConfig>> BooruProfile::authenticate(
    RequestContext context, AuthenticationData data) const {
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
	// merged url_params ride on every request (HttpClient applies config params).
	auto config = std::make_shared<ParserConfig>(*context.config());
	config->cookie_jar = nullptr;
	config->url_params[std::string(site_.auth->user_param)] = creds->username;
	config->url_params[std::string(site_.auth->key_param)]  = creds->password;
	co_return std::shared_ptr<const ParserConfig>(std::move(config));
}

AuthKeys BooruProfile::auth_keys() const noexcept {
	AuthKeys keys;
	if (site_.auth) {
		// The api key + user id url params are the durable credential to persist.
		keys.url_params = { std::string(site_.auth->key_param), std::string(site_.auth->user_param) };
	}
	return keys;
}

std::unique_ptr<ParserProfile> BooruParser::make_profile(ProfileInfo info) const {
    return std::make_unique<BooruProfile>(identifier(), std::move(info), site_);
}

} // namespace aniparse::engines
