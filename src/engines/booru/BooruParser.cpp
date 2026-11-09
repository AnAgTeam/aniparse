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

bool BooruParser::valid_for_url(const ParsedUrl& url) const {
	// The engine knows the site's URL scheme; a post or (where supported) a pool URL
	// is an image container routed to the images getter.
	const BooruEngine& engine = site_.engine();
	const bool is_container = engine.post_id_from_url(url).has_value()
	                       || (engine.supports_pools() && engine.pool_id_from_url(url).has_value());
	return is_container;
}

ParserCapabilities BooruParser::capabilities() const {
	// supports_images_store is derived, not declared: every booru parser hands out an
	// images getter (see make_images_getter), so the bit follows from the engine rather
	// than from a descriptor that can forget it.
	return { .flags = site_.capabilities | capabilities_flags::supports_images_store };
}

void BooruParser::emplace_domains(EmplaceDomainsSink& sink) const {
	for (std::string_view domain : site_.domains) {
		sink.add_domain(domain);
	}
}


namespace {
class BooruProfile final : public ParserProfile {
public:
    BooruProfile(ProfileInfo info, const BooruSite& site)
        : ParserProfile(std::move(info)), site_(site) {}
    void configure(ParserConfig& config) const override;
    std::span<const std::string_view> mirrors() const override;
    RequestTask<void> authenticate_impl(
	    AuthenticationSession& candidate,
	    AuthenticationData credentials) const override;
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

std::unique_ptr<ImagesGetter> BooruParser::make_images_getter() const {
	return std::make_unique<BooruImagesGetter>(site_);
}

RequestTask<void> BooruProfile::authenticate_impl(
    AuthenticationSession& candidate, AuthenticationData credentials) const {
	if (!site_.auth) {
		// An anonymous source has nothing to authenticate.
		co_return make_request_error(RequestErrorCode::NotImplemented, "The parser cannot auth");
	}

	const auto* creds = std::get_if<AuthenticationUserPassword>(&credentials);
	if (!creds) {
		co_return make_request_error(RequestErrorCode::InvalidArguments,
		    "booru credentials must be user id (username) + api key (password)");
	}

    auto* config = &candidate.config();
	config->url_params[std::string(site_.auth->user_param)] = creds->username;
	config->url_params[std::string(site_.auth->key_param)]  = creds->password;
	co_return {};
}

AuthKeys BooruProfile::auth_keys() const noexcept {
	AuthKeys keys;
	if (site_.auth) {
		// The api key + user id url params are the durable credential to persist.
		keys.url_params = { std::string(site_.auth->key_param), std::string(site_.auth->user_param) };
	}
	return keys;
}

std::unique_ptr<ParserProfile> BooruParser::make_profile_impl(ProfileInfo info) const {
    return std::make_unique<BooruProfile>(std::move(info), site_);
}

} // namespace aniparse::engines
