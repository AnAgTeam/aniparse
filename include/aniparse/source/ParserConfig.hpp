/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/Request.hpp"
#include "aniparse/net/CookieJar.hpp"
#include "aniparse/utility/FlagsBitfield.hpp"
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace aniparse {

class RequestContext;

/// Per-profile request switches.
using ParserConfigFlags = FlagsBitfield<64, struct ParserConfigFlagsTag>;
/// The bits of a ParserConfigFlags.
namespace parser_config_flags {
/// Follow HTTP redirects rather than surfacing the 3xx response. Honouring it is
/// up to the backend; the bundled curl one does not read it today, so a parser
/// cannot rely on toggling it.
inline constexpr auto follow_redirects = ParserConfigFlags::make_bit(0);
} // namespace parser_config_flags

/**
 * @brief Parser configuration for all its requests
 *
 * The per-profile half of the configuration (the transport-wide half is
 * @ref HttpClientConfig): whatever a parser needs stamped onto every request it makes —
 * its default headers and url params, the cookie jar holding its session, and its
 * request hooks. A @ref RequestContext applies it to each outgoing request.
 *
 * ParserProfile::make_session applies defaults and binds the session to its
 * parser and profile. User URL choices belong to SessionConfig. This value contains settings only; replacing it
 * cannot change a context's identity.
 * @see RequestContext
 */
struct ParserConfig {
	/// Default headers merged into every request. Headers already set on the request
	/// win: the config fills in only the names the request left unset.
	Headers headers;
	/// Query params appended to every request — the seam static credentials
	/// (an api key, a user id) ride on. @see ParserProfile::authenticate
	std::map<std::string, std::string> url_params;
	/// The session: the jar every request of this config reads and writes cookies
	/// through. Provisioned by the context (from the client backend) when null, so a
	/// derived config that carries a restored jar keeps it.
	std::shared_ptr<CookieJar> cookie_jar;
	/// Per-parser request switches. @see parser_config_flags
	ParserConfigFlags flags;

	/**
	 * @brief Request hooks run in order after merging headers and URL parameters.
	 * Each hook receives the operation's snapshot by reference, so computed values
	 * use the same pinned origins and settings as the request URL. Hooks may replace
	 * merged values to enforce protocol requirements, or sign the final request.
	 * @note Hooks run synchronously and may run concurrently for different requests.
	 * Do not retain either argument or capture a borrowed parser/profile receiver.
	 */
	std::vector<std::function<void(HttpRequest&, const RequestContext&)>> modifiers;
};

} // namespace aniparse
