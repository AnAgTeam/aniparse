/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/CookieJar.hpp"
#include "aniparse/Headers.hpp"

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

} // namespace aniparse
