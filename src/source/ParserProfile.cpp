/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/source/ParserProfile.hpp"
#include <algorithm>
#include "aniparse/net/ParsedUrl.hpp"
#include <boost/json.hpp>
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

RequestTask<std::shared_ptr<const ParserConfig>> ParserProfile::authenticate(
	RequestContext, AuthenticationData) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The parser cannot auth");
}

std::string web_login_origin(std::string_view url) {
    auto parsed = ParsedUrl::parse(url);
    if (!parsed || (parsed->scheme() != "http" && parsed->scheme() != "https")) {
        return {};
    }
    auto href = parsed->href();
    auto authority = href.substr(href.find("://") + 3);
    authority = authority.substr(0, authority.find_first_of("/?#"));
    if (auto at = authority.rfind('@'); at != std::string_view::npos) {
        authority.remove_prefix(at + 1);
    }
    return std::string(parsed->scheme()) + "://" + std::string(authority);
}

WebLoginObservationConfig ParserProfile::web_login_observation(RequestContext context) const {
    auto info = auth_info(context);
    if (!info || !info->web_login) {
        return {.events = {}, .storage = {}, .interval_ms = 0};
    }
    const auto& web = *info->web_login;
    auto config = web.observation;
    if (web.material == WebLoginInfo::Material::StorageToken && !web.storage_key.empty()) {
        config.storage.push_back({web.login_url, WebStorageKind::Local, web.storage_key});
    }
    for (auto& key : config.storage) {
        key.origin = web_login_origin(key.origin);
    }
    return config;
}

RequestTask<WebLoginCheck> ParserProfile::check_web_login(
    RequestContext context, WebLoginEventKind, WebLoginState state) const {
    auto info = auth_info(context);
    if (!info || !info->web_login || info->web_login->material != WebLoginInfo::Material::StorageToken) {
        co_return make_request_error(RequestErrorCode::NotImplemented,
                                    "This profile cannot verify browser sessions");
    }
    const auto& web = *info->web_login;
    const auto origin = web_login_origin(web.login_url);
    for (const auto& entry : state.storage) {
        if (origin.empty() || entry.location.origin != origin ||
            entry.location.storage != WebStorageKind::Local ||
            entry.location.key != web.storage_key || !entry.value || entry.value->empty()) {
            continue;
        }
        std::string token = *entry.value;
        if (!web.storage_token_path.empty()) {
            boost::system::error_code error;
            auto json = boost::json::parse(token, error);
            if (error) {
                continue;
            }
            const boost::json::value* value = &json;
            std::string_view path = web.storage_token_path;
            while (value && !path.empty()) {
                const auto dot = path.find('.');
                const auto key = path.substr(0, dot);
                value = value->is_object() ? value->as_object().if_contains(key) : nullptr;
                if (dot == std::string_view::npos) {
                    break;
                }
                path.remove_prefix(dot + 1);
            }
            if (!value || !value->is_string() || value->as_string().empty()) {
                continue;
            }
            token = std::string(value->as_string());
        }
        auto authenticated = co_await authenticate(context, AuthenticationToken{std::move(token), {}});
        if (!authenticated) co_return unexpected(std::move(authenticated.error()));
        if (!*authenticated) {
            co_return make_request_error(RequestErrorCode::UnexpectedResponse,
                                         "Authentication returned no configuration");
        }
        co_return InteractionCompleted{std::move(*authenticated)};
    }
    co_return InteractionPending{};
}

ParserProfile::ParserProfile(std::string parser_id, ProfileInfo info)
	: parser_id_(std::move(parser_id)), info_(std::move(info)) {
	if (parser_id_.empty() || info_.id.empty()) {
		throw std::invalid_argument("Parser and profile IDs must be nonempty");
	}
}

std::shared_ptr<ParserConfig> ParserProfile::make_config() const {
	auto config = std::make_shared<ParserConfig>();
	configure(*config);
	return config;
}

RequestContext ParserProfile::make_context(std::shared_ptr<const RuntimeServices> services) const {
	ParserConfig config;
	configure(config);
	return RequestContext::make(std::move(services), std::move(config), parser_id_, info_.id);
}

RequestContext ParserProfile::make_context(std::shared_ptr<HttpClient> client,
                                          std::shared_ptr<Logger> logger) const {
	return make_context(std::make_shared<RuntimeServices>(RuntimeServices{
	    .client = std::move(client), .logger = std::move(logger)}));
}

RequestContext ParserProfile::make_context(std::shared_ptr<const RuntimeServices> services,
                                          const AuthState& auth) const {
	auto context = make_context(std::move(services));
	import_auth(*context.config_, auth);
	return context;
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
