#include "aniparse/source/ProfileSession.hpp"
#include "aniparse/net/ParsedUrl.hpp"
#include "SessionAuthentication.hpp"
#include <algorithm>
#include <mutex>

namespace aniparse {

struct ProfileSession::Impl {
    std::shared_ptr<const RuntimeServices> services;
    std::shared_ptr<const OverridesState> pinned_overrides;
    std::shared_ptr<const RequestDefaults> pinned_defaults;
    bool pinned = false;
    mutable std::mutex mutex;
    std::shared_ptr<const detail::SessionState> state;
};

namespace {
Result<void> normalize_url(std::optional<std::string>& value, bool origin_only) {
    if (!value) return {};
    auto url = ParsedUrl::parse(*value);
    if (!url || (url->scheme() != "http" && url->scheme() != "https") || url->host().empty()) {
        return make_request_error(RequestErrorCode::InvalidArguments, "Expected an absolute HTTP(S) URL");
    }
    const auto href = url->href();
    constexpr std::string_view scheme_separator = "://";
    const auto authority_start = href.find(scheme_separator) + scheme_separator.size();
    const auto authority_end = href.find('/', authority_start);
    const auto authority = href.substr(authority_start, authority_end - authority_start);
    if (authority.find('@') != std::string_view::npos || href.find_first_of("?#") != std::string_view::npos ||
        (origin_only && url->path() != "/" && !url->path().empty())) {
        return make_request_error(RequestErrorCode::InvalidArguments,
            "Base URLs cannot contain credentials, query or fragment; frontend URLs must be origins");
    }
    *value = std::string(href);
    while (value->ends_with('/')) value->pop_back();
    return {};
}
void apply_user_settings(ParserConfig& config, const SessionConfig& settings) {
    Headers headers = settings.headers;
    headers.merge_missing(config.headers);
    config.headers = std::move(headers);
    for (const auto& [key, value] : settings.url_params) config.url_params[key] = value;
}

Result<void> validate_user_settings(const SessionConfig& settings, const AuthKeys& keys) {
    for (const auto* header : {"Authorization", "Proxy-Authorization", "Cookie"}) {
        if (settings.headers.contains(header)) {
            return make_request_error(RequestErrorCode::InvalidArguments, "Credentials belong to authentication, not user settings");
        }
    }
    for (const auto& name : keys.headers) {
        if (settings.headers.contains(name)) return make_request_error(RequestErrorCode::InvalidArguments, "Credential header in user settings");
    }
    for (const auto& name : keys.url_params) {
        if (settings.url_params.contains(name)) return make_request_error(RequestErrorCode::InvalidArguments, "Credential parameter in user settings");
    }
    return {};
}

}

ProfileSession ProfileSession::make(std::shared_ptr<const RuntimeServices> services,
                                    std::shared_ptr<const detail::ProfileBinding> binding,
                                    ParserConfig config, SessionConfig settings) {
    if (!services || !services->client) throw std::invalid_argument("Client has to be valid");
    if (!services->resources) {
        auto filled = std::make_shared<RuntimeServices>(*services);
        filled->resources = std::make_shared<ResourceCache>();
        services = std::move(filled);
    }
    if (!config.cookie_jar) config.cookie_jar = services->client->make_cookie_jar();
    auto impl = std::make_shared<Impl>();
    impl->services = std::move(services);
    impl->state = std::make_shared<const detail::SessionState>(detail::SessionState{
        std::move(binding), std::make_shared<const ParserConfig>(std::move(config)), std::move(settings)});
    return ProfileSession(std::move(impl));
}

ProfileSession ProfileSession::from_context(const RequestContext& context, bool pin) {
    auto impl = std::make_shared<Impl>();
    impl->services = context.services_;
    if (pin) impl->pinned_overrides = context.overrides_;
    impl->state = context.state_;
    impl->pinned = pin;
    if (pin) impl->pinned_defaults = context.defaults_;
    return ProfileSession(std::move(impl));
}

bool ProfileSession::is_pinned() const { return impl_->pinned; }

RequestContext ProfileSession::snapshot() const {
    std::shared_ptr<const detail::SessionState> state;
    {
        std::lock_guard lock(impl_->mutex);
        state = impl_->state;
    }
    auto overrides = impl_->pinned ? impl_->pinned_overrides : RequestContext::capture_overrides(*impl_->services);
    auto defaults = impl_->pinned ? impl_->pinned_defaults : impl_->services->client->request_defaults_snapshot();
    return RequestContext(impl_->services, std::move(state), std::move(overrides), std::move(defaults));
}

SessionConfig ProfileSession::settings() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->state->settings;
}

Result<void> ProfileSession::set_settings(SessionConfig settings) {
    if (impl_->pinned) return make_request_error(RequestErrorCode::InvalidArguments, "Login settings are pinned");
    if (auto checked = normalize_url(settings.base_url, false); !checked) return checked;
    if (auto checked = normalize_url(settings.canonical_base_url, true); !checked) return checked;
    std::lock_guard lock(impl_->mutex);
    const auto& current = *impl_->state;
    if (auto checked = validate_user_settings(settings, current.binding->auth_keys); !checked) return checked;
    if (settings == current.settings) {
        return {};
    }
    auto next = std::make_shared<detail::SessionState>(current);
    if (settings.headers != current.settings.headers || settings.url_params != current.settings.url_params) {
        auto config = std::make_shared<ParserConfig>(*current.config);
        // Replace the previous user overrides without dropping protocol state
        // established by authentication. URL-only changes share this config.
        for (const auto& [name, value] : current.settings.headers) {
            config->headers.erase(name);
            for (auto original : current.binding->defaults.headers.get_all(name)) {
                config->headers.append(name, std::string(original));
            }
        }
        for (const auto& [name, value] : current.settings.url_params) {
            config->url_params.erase(name);
            if (auto it = current.binding->defaults.url_params.find(name); it != current.binding->defaults.url_params.end()) {
                config->url_params.emplace(it->first, it->second);
            }
        }
        apply_user_settings(*config, settings);
        next->config = std::move(config);
    }
    next->settings = std::move(settings);
    impl_->state = std::move(next);
    return {};
}

Result<void> ProfileSession::set_mirror(std::optional<std::string> url) {
    if (impl_->pinned) return make_request_error(RequestErrorCode::InvalidArguments, "Login settings are pinned");
    if (auto checked = normalize_url(url, false); !checked) return checked;
    std::lock_guard lock(impl_->mutex);
    if (url == impl_->state->settings.base_url) {
        return {};
    }
    auto next = std::make_shared<detail::SessionState>(*impl_->state);
    next->settings.base_url = std::move(url);
    impl_->state = std::move(next);
    return {};
}

std::vector<std::string> ProfileSession::mirror_choices() const {
    auto context = snapshot();
    auto mirrors = context.mirrors();
    std::vector<std::string> choices;
    choices.reserve(mirrors.count());
    for (size_t index = 0; index < mirrors.count(); ++index) {
        choices.emplace_back(mirrors.base_url(index));
    }
    return choices;
}

std::shared_ptr<const ParserConfig> ProfileSession::config() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->state->config;
}

SessionState ProfileSession::export_state() const {
    auto context = snapshot();
    const auto& binding = *context.state_->binding;
    return {binding.parser_id, binding.profile_id, context.state_->settings,
            detail::export_auth(*context.state_->config, binding.auth_keys)};
}

AuthenticationSession::AuthenticationSession(RequestContext base)
    : config_(std::make_shared<ParserConfig>(base.state_->binding->defaults)), context_(std::move(base)) {
    apply_user_settings(*config_, context_.state_->settings);
    config_->cookie_jar = context_.services_->client->make_cookie_jar();
    auto state = std::make_shared<detail::SessionState>(*context_.state_);
    state->config = config_;
    context_.state_ = std::move(state);
    context_.resolve_urls();
}

ProfileSession AuthenticationSession::finish() {
    // Move out of the mutable builder: previously borrowed mutable references
    // never alias published settings. The jar itself remains a live cookie store.
    auto state = std::make_shared<detail::SessionState>(*context_.state_);
    state->config = std::make_shared<const ParserConfig>(std::move(*config_));
    context_.state_ = std::move(state);
    context_.resolve_urls();
    return ProfileSession::from_context(context_);
}

} // namespace aniparse
