/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/Headers.hpp"
#include "aniparse/utility/FlagsBitfield.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace aniparse {

/// Transport-wide switches.
using HttpClientFlags = FlagsBitfield<64, struct HttpClientFlagsTag>;
/// The bits of a HttpClientFlags.
namespace http_client_flags {
/// Validate the peer certificate and hostname on TLS connections. Clearing it
/// makes the transport accept any certificate — only meaningful for a debugging
/// proxy that terminates TLS.
inline constexpr auto verify_ssl = HttpClientFlags::make_bit(0);
} // namespace http_client_flags

/**
 * @brief An HTTP proxy every request of a @ref HttpClient is routed through.
 * Credentials are optional: leave @ref user and @ref passwd empty for a proxy that
 * needs no authentication.
 */
struct HttpProxy {
	std::string user;   ///< Proxy username; empty when the proxy takes no credentials.
	std::string passwd; ///< Proxy password; empty when the proxy takes no credentials.
	std::string host;   ///< Proxy hostname or address.
	uint16_t port = 0;  ///< Proxy port.
};

/**
 * @brief Application-wide HTTP defaults; source and request headers take priority.
 * @note Credentials belong in a profile configuration, not in these defaults.
 */
struct RequestDefaults {
    Headers headers; ///< Fallback headers, matched without regard to case.
};

/**
 * @brief Transport settings and application request defaults for a HttpClient.
 *
 * Applies to every request the client performs, whatever parser issued it — this is
 * the host application's knob (proxy, timeout, retry budget), as opposed to
 * @ref ParserConfig, which is per-parser and travels with the getter. An unset
 * optional leaves the backend's own default in place.
 * @see HttpClient::set_config
 */
struct HttpClientConfig {
    RequestDefaults request_defaults; ///< Applied by the common request context.

	/// Proxy to route every request through; unset for a direct connection.
	std::optional<HttpProxy> proxy;
	/// How many times a request is retried after a transport failure (timeout,
	/// connection, TLS) before the error is surfaced. A cancelled request is never
	/// retried, and an HTTP status is not a transport failure, so a 4xx/5xx response
	/// is returned rather than retried. Unset = the backend's default budget.
	std::optional<uint32_t> max_retries;
	/// Deadline for a single request; unset = the backend's default.
	std::optional<std::chrono::milliseconds> timeout;
	/// Transport-wide switches. @see http_client_flags
	HttpClientFlags flags;
};

} // namespace aniparse
