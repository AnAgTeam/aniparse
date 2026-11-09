/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/source/ParserProfile.hpp"
#include "aniparse/net/ParsedUrl.hpp"
#include <boost/json.hpp>
#include <stdexcept>

namespace aniparse {

RequestTask<void> ParserProfile::authenticate_impl(AuthenticationSession&, AuthenticationData) const {
    co_return make_request_error(RequestErrorCode::NotImplemented, "The profile cannot authenticate");
}

bool ParserProfile::owns(const RequestContext& context) const {
    return context.state_->binding->parser_id == parser_id_ && context.state_->binding->profile_id == info_.id;
}

RequestTask<ProfileSession> ParserProfile::authenticate(ProfileSession current, AuthenticationData credentials) const {
    return authenticate_context(current.snapshot(), std::move(credentials));
}

RequestTask<ProfileSession> ParserProfile::authenticate_context(RequestContext base, AuthenticationData credentials) const {
    if (!owns(base)) co_return make_request_error(RequestErrorCode::InvalidArguments, "Session belongs to another profile");
    AuthenticationSession candidate(std::move(base));
    auto result = co_await authenticate_impl(candidate, std::move(credentials));
    if (!result) co_return unexpected(std::move(result.error()));
    co_return candidate.finish();
}

ProfileSession ParserProfile::begin_web_login(ProfileSession current) const {
    auto base = current.snapshot();
    if (!owns(base)) throw std::invalid_argument("Session belongs to another profile");
    AuthenticationSession candidate(std::move(base));
    return ProfileSession::from_context(candidate.context_, true);
}

RequestTask<InteractionState<ProfileSession>> ParserProfile::check_web_login(
    ProfileSession login, WebLoginEventKind event, WebLoginState state) const {
    return check_web_login_context(login.snapshot(), event, std::move(state), login.is_pinned());
}

RequestTask<InteractionState<ProfileSession>> ParserProfile::check_web_login_context(
    RequestContext base, WebLoginEventKind event, WebLoginState state, bool pinned) const {
    if (!pinned) co_return make_request_error(RequestErrorCode::InvalidArguments, "Use begin_web_login before checking browser state");
    if (!owns(base)) co_return make_request_error(RequestErrorCode::InvalidArguments, "Session belongs to another profile");
    AuthenticationSession candidate(std::move(base));
    auto result = co_await check_web_login_impl(candidate, event, std::move(state));
    if (!result) co_return unexpected(std::move(result.error()));
    if (std::holds_alternative<InteractionPending>(*result)) co_return InteractionPending{};
    co_return InteractionCompleted{candidate.finish()};
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

WebLoginObservationConfig ParserProfile::web_login_observation(const ProfileSession& login) const {
    auto info = auth_info(login);
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

RequestTask<InteractionState<std::monostate>> ParserProfile::check_web_login_impl(
    AuthenticationSession& candidate, WebLoginEventKind, WebLoginState state) const {
    auto& context = candidate.context();
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
        auto authenticated = co_await authenticate_impl(candidate, AuthenticationToken{std::move(token), {}});
        if (!authenticated) co_return unexpected(std::move(authenticated.error()));
        co_return InteractionCompleted{std::monostate{}};
    }
    co_return InteractionPending{};
}

} // namespace aniparse
