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
 * selected mirror. A @ref RequestContext applies it to each outgoing request.
 *
 * A config for a given parser is not written by hand: it is *derived*, by handing a
 * profile to create a fresh config with ParserProfile::make_config and adopting it via
 * RequestContext::new_with_config. That derivation is what stamps @ref parser_id and
 * seeds the parser's defaults, and it is mandatory before any getter of that parser
 * runs — see RequestContext::new_with_config.
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

	/// Identifier of the parser this config was derived for, stamped by
	/// ParserProfile::make_config. Lets request-time services keyed by parser (mirror
	/// overrides) find this parser's entry without the getter naming its own id.
	/// Empty on a config that was not derived through make_config.
	std::string parser_id;
	std::string profile_id; ///< Stable profile ID, stamped by ParserProfile::make_config.

	/// Selected alternative link (mirror) index. @see AltLink, RequestContext::alt_link
	size_t alt_link = 0;

	/// Last-mile hooks run on every outgoing request, after the headers and url params
	/// above have been applied, in order. The escape hatch for a request property that
	/// is computed rather than constant (a signature over the final URL, say).
	std::vector<std::function<void(aniparse::HttpRequest&)>> modifiers;
};

} // namespace aniparse
