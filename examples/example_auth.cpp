// Offline token login: a clean candidate, explicit session adoption and persistence.
#include "ExampleClient.hpp"
#include <aniparse/source/Parser.hpp>
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
protected:
    RequestTask<void> authenticate_impl(
        AuthenticationSession& candidate, AuthenticationData credentials) const override {
        const auto* token = std::get_if<AuthenticationToken>(&credentials);
        // Synthetic credential for this offline fixture only. A real source
        // verifies it with requests through candidate.context().
        if (!token || token->token != "example-token") {
            co_return make_request_error(RequestErrorCode::InvalidCredentials, "Invalid example token");
        }
        candidate.config().headers.set("Authorization", "Bearer " + token->token);
        co_return {};
    }
};

class TokenParser final : public Parser {
public:
    ParserInfo info() const override { return {.name = "Example"}; }
    std::string identifier() const override { return "Example"; }
    void emplace_domains(EmplaceDomainsSink&) const override {}
protected:
    std::unique_ptr<ParserProfile> make_profile_impl(ProfileInfo info) const override {
        return std::make_unique<TokenProfile>(std::move(info));
    }
};

int main() {
    TokenParser parser;
    auto profile = parser.make_profile(default_profile_id);
    auto services = examples::make_services();
    auto anonymous = profile->make_session(services);
    // profile stays alive until sync_wait completes: the task borrows it.
    auto authenticated = coro::sync_wait(profile->authenticate(
        anonymous, AuthenticationToken{.token = "example-token"}));
    if (!authenticated || anonymous.config()->headers.contains("Authorization")) return 1;
    auto active = std::move(*authenticated);
    auto saved = active.export_state();
    // Store saved securely under its parser_id/profile_id, never under a catalog ID.
    // Catalogs sharing that profile may use active.snapshot().
    auto restored = profile->restore_session(services, saved);
    if (!restored || restored->config()->headers.get("Authorization") != "Bearer example-token") return 1;
    auto rejected = coro::sync_wait(profile->authenticate(active, AuthenticationToken{.token = "bad"}));
    if (rejected || rejected.error().code != RequestErrorCode::InvalidCredentials) return 1;
    if (active.config()->headers.get("Authorization") != "Bearer example-token") return 1;
    std::println("Login restored; anonymous and existing sessions remain unchanged.");
}
