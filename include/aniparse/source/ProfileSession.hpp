#pragma once
#include "aniparse/runtime/RequestContext.hpp"
#include "aniparse/source/Authentication.hpp"

namespace aniparse {

/**
 * @brief Portable session data; contains no services, callbacks or live jar.
 */
struct SessionState {
    std::string parser_id;
    std::string profile_id;
    SessionConfig settings;
    AuthState auth;
};

/**
 * @brief A profile/account session publishing immutable operation settings.
 * Copies share settings updates and cookies. Authentication returns an independent
 * session. Snapshots retain their old settings and overrides after an update.
 * No profile or parser receiver is borrowed by this object.
 */
class ProfileSession {
public:
    /**
     * @brief Capture the current settings, overrides and application headers.
     * @return An owning operation context; no configuration tables are copied.
     */
    [[nodiscard]] RequestContext snapshot() const;
    /**
     * @brief Capture a snapshot when passing this session to an operation.
     * @return An owning context with the same semantics and cost as snapshot().
     * @note Each conversion captures independently, at the call site before a
     * coroutine begins execution. A deferred task retains that snapshot. Use
     * snapshot() explicitly to share one environment across multiple operations.
     */
    [[nodiscard]] operator RequestContext() const { return snapshot(); }
    /**
     * @return An owned copy of the current user settings.
     */
    [[nodiscard]] SessionConfig settings() const;
    /**
     * @brief Publish normalized user choices for subsequent operations.
     * @param settings New settings; absent URLs use automatic selection.
     * Credential header/parameter names are reserved for authentication.
     * @return Success, or InvalidArguments without changing the session.
     */
    Result<void> set_settings(SessionConfig settings);
    /**
     * @brief Select an explicit fetch URL, or restore automatic mirror selection.
     * @param url Absolute HTTP(S) base URL, or nullopt for automatic selection.
     * @return Success, or InvalidArguments without changing the session.
     */
    Result<void> set_mirror(std::optional<std::string> url);
    /**
     * @return Current remote/built-in mirror choices, without changing selection.
     */
    [[nodiscard]] std::vector<std::string> mirror_choices() const;
    /**
     * @return Portable user choices and the profile's declared durable credentials.
     */
    [[nodiscard]] SessionState export_state() const;
    /**
     * @return An immutable view of effective request settings and the live cookie jar.
     */
    [[nodiscard]] std::shared_ptr<const ParserConfig> config() const;

private:
    friend class ParserProfile;
    friend class AuthenticationSession;
    struct Impl;
    bool is_pinned() const;
    explicit ProfileSession(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}
    static ProfileSession make(std::shared_ptr<const RuntimeServices> services,
                               std::shared_ptr<const detail::ProfileBinding> binding,
                               ParserConfig config, SessionConfig settings = {});
    static ProfileSession from_context(const RequestContext& context, bool pin = false);
    std::shared_ptr<Impl> impl_;
};

/**
 * @brief Private mutable candidate supplied to a profile's authentication hook.
 * Only the immediately awaited hook may access it. Do not retain references,
 * contexts, or background tasks beyond that hook. Requests share the candidate
 * jar and one pinned overrides generation. The active session is unaffected.
 */
class AuthenticationSession {
public:
    AuthenticationSession(const AuthenticationSession&) = delete;
    AuthenticationSession& operator=(const AuthenticationSession&) = delete;
    /**
     * @return Mutable candidate request settings; valid only during the hook.
     */
    ParserConfig& config() noexcept { return *config_; }
    /**
     * @return Context for login requests; do not mutate config during concurrent requests.
     */
    RequestContext& context() noexcept { return context_; }
private:
    friend class ParserProfile;
    explicit AuthenticationSession(RequestContext base);
    ProfileSession finish();
    std::shared_ptr<ParserConfig> config_;
    RequestContext context_;
};

} // namespace aniparse
