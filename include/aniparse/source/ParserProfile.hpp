/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/runtime/RequestContext.hpp"
#include "aniparse/source/ProfileSession.hpp"
#include "aniparse/source/Authentication.hpp"
#include "aniparse/source/ProfileInfo.hpp"

namespace aniparse {

/**
 * @brief Immutable configuration and authentication behavior shared by catalogs.
 *
 * Account state lives in ProfileSession, not in this object. Store authentication by
 * the pair of parser ID and profile ID. Multiple sessions may use one profile
 * with independent accounts; sharing a profile alone does not share cookies.
 */
class ParserProfile {
public:
    /**
     * @brief Construct profile behavior; Parser::make_profile binds its owner.
     * @param info Selected profile descriptor.
     */
    explicit ParserProfile(ProfileInfo info);

    /**
     * @brief Create an independent anonymous session with this profile's defaults.
     * @param services Shared services; the client must be non-null.
     * @param settings User choices; absent URLs use current overrides/built-ins.
     * @return A session that may outlive this profile.
     * @throws std::invalid_argument For missing identity/client or invalid URLs.
     */
    ProfileSession make_session(std::shared_ptr<const RuntimeServices> services,
                                SessionConfig settings = {}) const;
    /**
     * @brief Restore portable credentials over current profile defaults.
     * @param services Shared services with a non-null client.
     * @param saved Saved state for this exact parser/profile pair.
     * @return Independent session, or InvalidArguments for mismatched/invalid state.
     */
    Result<ProfileSession> restore_session(std::shared_ptr<const RuntimeServices> services,
                                           SessionState saved) const;
    /**
     * @brief Authenticate on an isolated candidate inheriting only user choices.
     * @param current Existing session, left unchanged including its cookies.
     * @param credentials Owned credentials for this attempt.
     * @return A new independent session on success; an error leaves current intact.
     * @note Keep this profile alive until the task completes or is destroyed.
     */
    RequestTask<ProfileSession> authenticate(ProfileSession current, AuthenticationData credentials) const;
    /**
     * @brief Start an isolated web interaction with pinned origins and request defaults.
     * @param current Session whose user choices should be inherited.
     * @return Login session to retain from opening the browser through all checks.
     * @throws std::invalid_argument If the session belongs to another profile.
     */
    ProfileSession begin_web_login(ProfileSession current) const;
    /**
     * @brief Resolve login presentation from a session's current/pinned snapshot.
     * @param session Ordinary session or the pinned browser interaction session.
     * @return The profile's resolved authentication descriptor.
     */
    std::optional<AuthInfo> auth_info(const ProfileSession& session) const { return auth_info(session.snapshot()); }
    /**
     * @brief Resolve browser observation settings from the pinned login session.
     * @param login Session returned by begin_web_login.
     * @return Owned observation settings for that interaction.
     */
    WebLoginObservationConfig web_login_observation(const ProfileSession& login) const;
    /**
     * @brief Verify browser state using the origins pinned by begin_web_login.
     * @param login Login session retained for the browser interaction.
     * @param event Reason for the check.
     * @param state Owned browser state.
     * @return Pending, or a new session for explicit adoption; errors leave login intact.
     * @note Keep this profile alive until the task completes or is destroyed.
     */
    RequestTask<InteractionState<ProfileSession>> check_web_login(
        ProfileSession login, WebLoginEventKind event, WebLoginState state) const;

	virtual ~ParserProfile() = default;

	/**
	 * @brief Read this profile's immutable descriptor.
	 * @return A reference valid for the lifetime of this profile.
	 */
	const ProfileInfo& info() const noexcept { return info_; }

	/**
	 * @brief Read the owning parser identity.
	 * @return An owned copy, independent of this profile's lifetime.
	 */
	std::string parser_id() const { return parser_id_; }

	/**
	 * @see AuthInfo
	 * @brief How a user signs in to this source — the flow the client drives.
	 *
	 * Consulted BEFORE any login: it tells the client which AuthenticationData
	 * variant to build (token vs username/password) and which UI to present, so the
	 * client never guesses. Pure and side-effect-free (no network): a login_url that
	 * must be computed is resolved lazily when the web view opens, not here. Default:
	 * empty (the source needs no authentication).
	 * @return The sign-in descriptor, or nullopt when the source is anonymous.
	 */
	[[nodiscard]] virtual std::optional<AuthInfo> auth_info() const;

	/**
	 * @brief Resolve this source's sign-in descriptor against a request context.
	 *
	 * The default returns the static descriptor from @ref auth_info. Sources with
	 * a frontend origin controlled by overrides may override it to select a login URL
	 * from the context's immutable overrides snapshot.
	 * @param context Parser-configured request context; implementations must not
	 * perform network I/O or mutate it.
	 * @return The resolved sign-in descriptor, or nullopt when the source is
	 * anonymous.
	 * @note The context is passed by value so the result cannot borrow a caller's
	 * short-lived context across an asynchronous UI boundary.
	 */
	[[nodiscard]] virtual std::optional<AuthInfo> auth_info(RequestContext context) const;

	/**
	 * @see AuthKeys
	 * @brief Names of the credential-bearing config entries to persist.
	 *
	 * These identify which cookies / headers / url params of the authenticated
	 * config are the durable credential and should be copied into an AuthState;
	 * everything else (volatile session/anti-bot data) is left behind. Default:
	 * derived from @ref auth_info's fields (their targets grouped by channel), so a
	 * parser that declares auth_info gets this for free; empty when there is no
	 * auth_info. Override only for a credential a field list cannot name (e.g. the
	 * session cookie minted by a username/password login).
	 * @return The names of the durable credential entries; empty when none.
	 */
	virtual AuthKeys auth_keys() const noexcept;


	/**
	 * @brief The source's built-in mirrors (full base URLs), in selection order.
	 *
	 * One mirror set serves every catalog referencing this profile. The single source of truth a getter's fetch path
	 * (RequestContext::base_url) and the mirror choices (ProfileSession::mirror_choices) both read;
	 * a remote override keyed by parser and profile may supersede it when
	 * the next operation snapshot is captured. Default: none.
	 * @return The built-in base URLs; empty for a source with no selectable mirrors.
	 */
	[[nodiscard]] virtual std::span<const std::string_view> mirrors() const;

	/**
	 * @brief Declare this profile's built-in public frontend origin.
	 * @return An owned origin URL, or an empty string when none is declared.
	 * @note Pure metadata used by overrides generation. Request-time code resolves
	 * overrides through the no-argument RequestContext::canonical_base_url().
	 */
	[[nodiscard]] virtual std::string canonical_base_url() const;

protected:
	/**
	 * @brief Seed parser-specific defaults onto a freshly derived config.
	 *
	 * Called when capturing defaults for a new session,
	 * so a parser need not repeat these per request. Default:
	 * no-op. Set constant headers/params directly and register request rules in
	 * ParserConfig::modifiers. Those rules receive the operation's snapshot when
	 * a request is sent, so derived headers follow its resolved origins.
	 * These defaults belong to the profile and are shared by its catalogs.
	 * @param config Config being readied, mutated in place.
	 */
	virtual void configure(ParserConfig& config) const;

    /**
     * @brief Implement the login protocol on an already clean candidate.
     * @param candidate Mutable settings and pinned request context owned by the wrapper.
     * @param credentials Owned credentials.
     * @return Success after configuring/verifying the candidate, or an error.
     * @note Immediately awaited by authenticate. Borrowed candidate lives in its
     * parent coroutine frame; do not retain it or launch work beyond this call.
     */
    virtual RequestTask<void> authenticate_impl(AuthenticationSession& candidate,
                                                 AuthenticationData credentials) const;
    /**
     * @brief Verify a browser sample on an isolated candidate.
     * @param candidate Clean candidate owned by the awaiting wrapper.
     * @param event Reason for the check.
     * @param state Owned browser sample.
     * @return Pending or completed with no payload; the wrapper returns the session.
     * @note Candidate references and child tasks must not escape this call.
     */
    virtual RequestTask<InteractionState<std::monostate>> check_web_login_impl(
        AuthenticationSession& candidate, WebLoginEventKind event, WebLoginState state) const;
	ParserProfile(const ParserProfile&) = default;
	ParserProfile(ParserProfile&&) = default;
	ParserProfile& operator=(const ParserProfile&) = default;
	ParserProfile& operator=(ParserProfile&&) = default;

private:
    friend struct Parser;
    std::shared_ptr<const detail::ProfileBinding> make_binding() const;
    bool owns(const RequestContext& context) const;
    RequestTask<ProfileSession> authenticate_context(RequestContext base, AuthenticationData credentials) const;
    RequestTask<InteractionState<ProfileSession>> check_web_login_context(
        RequestContext base, WebLoginEventKind event, WebLoginState state, bool pinned) const;
	std::string parser_id_;
	ProfileInfo info_;
};
} // namespace aniparse
