/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/net/CookieJar.hpp"
#include "aniparse/types/Headers.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <variant>
#include <vector>

/**
 * @file
 * Credentials as data: what a source needs to identify a user (a token, or a
 * username/password) and the AuthState that carries the result of signing in.
 * A parser declares which config entries hold its credentials (AuthKeys) instead
 * of hiding them in bespoke fields, so an authenticated session can be exported
 * and restored without the caller knowing how a given source authenticates.
 */

namespace aniparse {

struct ParserConfig;

/**
 * @brief Data for authentification using username and password
 */
struct AuthenticationUserPassword {
	std::string username;
	std::string password;
	bool requires_2fa = false;
};

/**
 * @brief Data for authentification using token
 *        (some string representing all required information
 *        to identify user)
 */
struct AuthenticationToken {
	std::string token;
	std::string type;
};

/**
 * @brief Data that can be used for authentification
 */
using AuthenticationData = std::variant<AuthenticationUserPassword, AuthenticationToken>;

/**
 * @brief Names of the credential-bearing entries to lift out of a ParserConfig.
 *
 * A parser declares which cookies / headers / url params of its (authenticated)
 * config are the durable credential, so the library can copy exactly those into
 * an AuthState. Everything volatile (session ids, anti-bot cookies, User-Agent,
 * ...) is left behind. @see Parser::auth_keys, Parser::export_auth
 */
struct AuthKeys {
	std::vector<std::string> cookies;
	std::vector<std::string> headers;
	std::vector<std::string> url_params;
};

/**
 * @brief A persistable authentication session distilled from a ParserConfig.
 *
 * Carries only the credential-bearing entries (per AuthKeys) across the three
 * request channels, plus the mirror the session belongs to. Portable by design
 * (structured Cookie), so it can be stored and later merged back into a config.
 * @see Parser::export_auth, import_auth
 */
struct AuthState {
	std::vector<Cookie> cookies;
	Headers headers;
	std::map<std::string, std::string> url_params;
	size_t alt_link = 0;
};

/**
 * @brief Merge a saved AuthState back into a config (the inverse of export_auth).
 *
 * Generic and parser-independent: sets the cookies on the config's jar, merges
 * the auth headers and url params, and restores the mirror index. The config
 * must already own a cookie jar for cookies to be restored.
 * @param config Config to authenticate in place
 * @param state Previously exported session
 */
void import_auth(ParserConfig& config, const AuthState& state);

/**
 * @brief How a source expects a user to sign in — the shape the client collects.
 * Declared by @ref Parser::auth_info so the client picks the right
 * @ref AuthenticationData variant and UI instead of guessing.
 */
enum class AuthMethod {
	/// A username + password fed to Parser::authenticate_context, which logs in and
	/// returns an authenticated config. The client shows a standard credentials form.
	UsernamePassword,
	/// An interactive in-browser login (captcha / social OAuth); the client opens a
	/// web view at AuthInfo::login_url and lifts the credential per AuthInfo::material,
	/// then hands it back as an AuthenticationToken.
	InteractiveWeb,
	/// A token / API key the user pastes; the client collects AuthInfo::fields and
	/// builds an AuthenticationToken.
	Token,
};

/**
 * @brief One *injected* credential slot: what to show the user (or lift from a web
 * login) and where the value lands on every request.
 *
 * Describes a directly-injected credential — a token, an API key, a session
 * cookie — not the username/password fed to an interactive login (those drive a
 * handshake, they are not injected). @ref auth_keys_from_fields turns a list of
 * these into the @ref AuthKeys a session persists by, so a parser declares its
 * credential shape once.
 */
struct AuthField {
	/// Which request channel the value is carried on — the axis AuthKeys groups by.
	enum class Channel { Header, Cookie, UrlParam };

	/// Stable identifier of the slot (e.g. "api_key", "user_id", "token"). Not shown.
	std::string key;
	/// Human-readable label for the input field (e.g. "API key"). Empty for a lifted value.
	std::string label;
	/// The channel the value is carried on.
	Channel channel = Channel::Header;
	/// The header / cookie / url-param name the value becomes.
	std::string target;
	/// How the value is wrapped into the entry; the first "{}" is replaced by the raw
	/// value (e.g. "Bearer {}" for an Authorization header). Default: the raw value.
	std::string value_template = "{}";
	/// Whether the input is a secret and should be masked. Default: true.
	bool secret = true;
};

/**
 * @brief What a source needs to sign a user in: the method to drive the UI, the
 * credential slots to collect/inject, and the interactive details when a browser
 * is required. Pure data, no I/O — the client reads it up front to decide the flow.
 * @see Parser::auth_info
 */
struct AuthInfo {
	/// The sign-in method the source uses.
	AuthMethod method = AuthMethod::UsernamePassword;

	/// Injected credential slots (Token / multi-field keys / the lifted web token).
	/// Empty for UsernamePassword, whose session credential is named by auth_keys().
	std::vector<AuthField> fields;
	/// UsernamePassword: whether the source may additionally demand a 2FA step.
	bool requires_2fa = false;

	// --- InteractiveWeb only ---
	/// Where the value the fields inject is lifted from once the web login completes.
	enum class Material { Cookies, StorageToken };

	/// The URL the web view opens for the user to sign in.
	std::string login_url;
	/// URL prefix(es) whose navigation signals a completed login.
	std::vector<std::string> success_globs;
	/// Where the credential is lifted from after login.
	Material material = Material::Cookies;
	/// StorageToken: the localStorage key the token sits under.
	std::string storage_key;
	/// StorageToken: dot-path to the token inside the JSON value at @ref storage_key
	/// (e.g. "session.access_token" when the stored value is a JSON object nesting the
	/// token). Empty when the stored value is the raw token string.
	std::string storage_token_path;
};

/**
 * @brief Derive the persistable @ref AuthKeys from a credential-field list by
 * grouping each field's target under its channel. Lets a parser declare its
 * credential shape once (in AuthInfo::fields) and get auth_keys() for free.
 */
AuthKeys auth_keys_from_fields(const std::vector<AuthField>& fields);

} // namespace aniparse
