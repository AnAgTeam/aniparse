/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/ClientContext.hpp"
#include "aniparse/types/Authentication.hpp"
#include "aniparse/types/ProfileInfo.hpp"
#include "aniparse/manga/Manga.hpp"

namespace aniparse {

/**
 * @brief Immutable configuration and authentication behavior shared by catalogs.
 *
 * User state lives in ParserConfig, not in this object. Store authentication by
 * the pair of parser ID and profile ID. Multiple contexts may use one profile
 * with independent accounts; sharing a profile alone does not share cookies.
 */
class ParserProfile {
public:
	/**
	 * @brief Bind this profile to its owning parser and descriptor.
	 * @param parser_id Stable owning parser identifier.
	 * @param info Owned profile descriptor with a nonempty ID.
	 * @throws std::invalid_argument If either identifier is empty.
	 */
	explicit ParserProfile(std::string parser_id, ProfileInfo info);
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
	 * @brief Authenticate the parser's service with the given credentials.
	 *
	 * Login belongs to this profile: the returned config may be used by every
	 * catalog referencing this profile.
	 *
	 * Contract: returns a FRESH authenticated config and does NOT mutate the
	 * passed context. Implementations log in on a new config with its own cookie
	 * jar (copy the config, clear its cookie_jar, adopt it via new_with_config,
	 * and perform the login through that context), so the caller's session is left
	 * untouched — retries and failed logins never pollute it. The caller adopts
	 * the result via RequestorContext::new_with_config. By default reports
	 * NotImplemented.
	 * @param context Request context with this parser/profile identity (left unmodified)
	 * @param data Credentials to authenticate with
	 * @return Task with a new authenticated config for the caller to adopt.
	 * @note The caller must keep this profile alive until the returned task has
	 * completed or been destroyed. The task borrows this; it does not own the
	 * profile. This also applies when a task is stored or returned to another caller.
	 */
	virtual NetworkRequestTask<std::shared_ptr<const ParserConfig>> authenticate_context(
	    RequestorContext context,
	    AuthenticationData data) const;

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
	 * a catalog-controlled frontend origin may override it to select a login URL
	 * from the context's immutable catalog snapshot.
	 * @param context Parser-configured request context; implementations must not
	 * perform network I/O or mutate it.
	 * @return The resolved sign-in descriptor, or nullopt when the source is
	 * anonymous.
	 * @note The context is passed by value so the result cannot borrow a caller's
	 * short-lived context across an asynchronous UI boundary.
	 */
	[[nodiscard]] virtual std::optional<AuthInfo> auth_info(RequestorContext context) const;

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
	 * @see AuthState
	 * @brief Distill a persistable session from an authenticated config.
	 *
	 * Default: copy the entries named by auth_keys() out of the config. Override
	 * only for exotic credentials that a name list cannot express (e.g. a token
	 * parsed out of a response body).
	 * @param config Authenticated config to export from
	 * @return The persistable session
	 */
	virtual AuthState export_auth(const ParserConfig& config) const;

	/**
	 * @brief Create a fresh configuration for this profile.
	 * @return An independent config with parser/profile identity and configured
	 * defaults; the context provisions its cookie jar when adopted.
	 * @note Does not inherit credentials or client request defaults. Restore saved
	 * authentication explicitly with import_auth after creation.
	 */
	[[nodiscard]] std::shared_ptr<ParserConfig> make_config() const;

	/**
	 * @brief Seed parser-specific defaults onto a freshly derived config.
	 *
	 * Called by @ref make_config after the common derivation (fresh session,
	 * identity stamp), so a parser need not repeat these per request. Default:
	 * no-op. Override to set constant headers/params a parser needs on every
	 * request (e.g. an API's fixed headers). These defaults belong to the profile
	 * and are shared by its catalogs.
	 * @param config Config being readied, mutated in place.
	 */
	virtual void configure(ParserConfig& config) const;

	/**
	 * @brief The source's built-in mirrors (full base URLs), in selection order.
	 *
	 * One mirror set serves every catalog referencing this profile. The single source of truth a getter's fetch path
	 * (RequestorContext::base_url) and the UI picker (@ref mirror_choices) both read;
	 * a live catalog override keyed by parser and profile may supersede it at
	 * request time. Default: none.
	 * @return The built-in base URLs; empty for a source with no selectable mirrors.
	 */
	[[nodiscard]] virtual std::span<const std::string_view> mirrors() const;

	/**
	 * @brief Declare this profile's built-in public frontend origin.
	 * @return An owned origin URL, or an empty string when none is declared.
	 * @note Pure metadata used by catalog generation. Request-time code resolves
	 * overrides through RequestorContext::canonical_base_url with its fallback.
	 */
	[[nodiscard]] virtual std::string canonical_base_url() const;

	/**
	 * @brief The mirror choices to show the user, with any live catalog override
	 * applied.
	 *
	 * The built-in @ref mirrors overlaid with the override carried in @p context
	 * (keyed by this parser's identity, stamped into the config by @ref make_config)
	 * — so a picker reflects the mirrors actually fetched from, not a stale list.
	 * @param context Context carrying the current mirror snapshot and parser id.
	 * @return The resolved mirror descriptors, in selection order.
	 */
	[[nodiscard]] std::vector<AltLink> mirror_choices(const RequestorContext& context) const;


private:
	std::string parser_id_;
	ProfileInfo info_;
};
} // namespace aniparse
