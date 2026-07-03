/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/types/Authentication.hpp"
#include "aniparse/ClientContext.hpp"

namespace aniparse {

void import_auth(ParserConfig& config, const AuthState& state) {
	if (config.cookie_jar) {
		for (const Cookie& cookie : state.cookies) {
			config.cookie_jar->set_cookie(cookie);
		}
	}
	for (const auto& [name, value] : state.headers) {
		config.headers[name] = value;
	}
	for (const auto& [name, value] : state.url_params) {
		config.url_params[name] = value;
	}
	config.alt_link = state.alt_link;
}

} // namespace aniparse
