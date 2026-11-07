// Offline token login: descriptor, fresh configuration, adoption and persistence.
#include "ExampleClient.hpp"
#include <aniparse/source/ParserProfile.hpp>
#include <coro/sync_wait.hpp>
#include <print>

using namespace aniparse;

class TokenProfile final : public ParserProfile {
public:
    using ParserProfile::ParserProfile;
    std::optional<AuthInfo> auth_info() const override {
        return AuthInfo{.method = AuthMethod::Token, .fields = {{
            .key = "token", .label = "API token", .channel = AuthField::Channel::Header,
            .target = "Authorization", .value_template = "Bearer {}",
        }}};
    }
    RequestTask<std::shared_ptr<const ParserConfig>> authenticate(
        RequestContext context, AuthenticationData credentials) const override {
        const auto* token = std::get_if<AuthenticationToken>(&credentials);
        // Synthetic credential for this offline fixture only. A real source
        // verifies credentials against its service using a fresh login context.
        if (!token || token->token != "example-token")
            co_return make_request_error(RequestErrorCode::InvalidCredentials, "Invalid example token");
        auto config = std::make_shared<ParserConfig>(*context.config());
        config->cookie_jar.reset();
        config->headers.set("Authorization", "Bearer " + token->token);
        auto login = context.new_with_config(config); // Provisions an independent jar.
        co_return login.config();
    }
};

int main() {
    TokenProfile profile("Example", {"account", "Shared account"});
    auto client = examples::make_client();
    auto services = std::make_shared<RuntimeServices>();
    services->client = client;
    auto anonymous = profile.make_context(services);
    // profile stays alive until sync_wait completes: the task borrows it.
    auto authenticated = coro::sync_wait(profile.authenticate(
        anonymous, AuthenticationToken{.token = "example-token"}));
    if (!authenticated || anonymous.config()->headers.contains("Authorization")) return 1;
    auto active = anonymous.new_with_config(*authenticated);
    auto saved = profile.export_auth(*active.config());
    // Store saved securely under ("Example", "account"), never under a catalog ID.
    // Catalogs sharing that profile may use active; another account gets a new context.
    auto restored = profile.make_context(services, saved);
    if (restored.config()->headers.get("Authorization") != "Bearer example-token") return 1;
    auto rejected = coro::sync_wait(profile.authenticate(active, AuthenticationToken{.token = "bad"}));
    if (rejected || rejected.error().code != RequestErrorCode::InvalidCredentials) return 1;
    if (active.config()->headers.get("Authorization") != "Bearer example-token") return 1;
    std::println("Login restored; anonymous context and existing session remain unchanged.");
}
