/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/source/ParserProfile.hpp"
#include "SessionAuthentication.hpp"
#include <stdexcept>

namespace aniparse {

ParserProfile::ParserProfile(ProfileInfo info) : info_(std::move(info)) {
    if (info_.id.empty()) throw std::invalid_argument("Profile ID must be nonempty");
}

std::shared_ptr<const detail::ProfileBinding> ParserProfile::make_binding() const {
    if (parser_id_.empty()) throw std::invalid_argument("Create profiles through Parser::make_profile");
    auto binding = std::make_shared<detail::ProfileBinding>();
    binding->parser_id = parser_id_;
    binding->profile_id = info_.id;
    configure(binding->defaults);
    binding->defaults.cookie_jar.reset();
    binding->auth_keys = auth_keys();
    for (auto url : mirrors()) binding->mirrors.emplace_back(url);
    for (const auto& url : binding->mirrors) binding->mirror_views.emplace_back(url);
    binding->canonical_base_url = canonical_base_url();
    return binding;
}

ProfileSession ParserProfile::make_session(std::shared_ptr<const RuntimeServices> services,
                                           SessionConfig settings) const {
    auto binding = make_binding();
    auto config = binding->defaults;
    auto session = ProfileSession::make(std::move(services), std::move(binding), std::move(config));
    if (auto result = session.set_settings(std::move(settings)); !result) {
        throw std::invalid_argument(result.error().message);
    }
    return session;
}

Result<ProfileSession> ParserProfile::restore_session(std::shared_ptr<const RuntimeServices> services,
                                                      SessionState saved) const {
    if (saved.parser_id != parser_id_ || saved.profile_id != info_.id) {
        return make_request_error(RequestErrorCode::InvalidArguments, "Saved session belongs to another profile");
    }
    auto session = make_session(std::move(services));
    if (auto checked = session.set_settings(std::move(saved.settings)); !checked) {
        return unexpected(std::move(checked.error()));
    }
    auto context = session.snapshot();
    auto config = std::make_shared<ParserConfig>(*context.config());
    detail::import_auth(*config, saved.auth);
    auto state = std::make_shared<detail::SessionState>(*context.state_);
    state->config = std::move(config);
    context.state_ = std::move(state);
    context.resolve_urls();
    return ProfileSession::from_context(context);
}

void ParserProfile::configure(ParserConfig&) const {}

std::span<const std::string_view> ParserProfile::mirrors() const {
	return {};
}

std::string ParserProfile::canonical_base_url() const {
	return {};
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

} // namespace aniparse
