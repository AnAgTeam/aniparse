/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <stdexcept>
#include <string>

namespace aniparse {

/**
 * @brief Backend-neutral classification of a transport failure.
 * Coarse enough to be produced by any client backend (curl, NSURLSession, ...)
 * without leaking its native error codes.
 */
enum class NetErrc {
	/// No error (reserved; a NetError is never thrown with this code).
	ok,
	/// The request exceeded its timeout.
	timed_out,
	/// The request was cancelled on purpose (stop requested).
	cancelled,
	/// Could not establish the connection (resolve/connect failed).
	connect_failed,
	/// The TLS handshake or certificate verification failed.
	tls_failed,
	/// The server responded, but with an HTTP status treated as an error.
	/// @see NetError::http_status
	http_error,
	/// Any other transport failure.
	other,
};

/**
 * @brief Backend-neutral transport error thrown by the client.
 * Replaces the curl-bound exception (asyncnet::NetworkRuntimeError) at the
 * client boundary so the domain layer never catches a curlpp type. Each backend
 * maps its native failure onto a @ref NetErrc; the message keeps the backend's
 * human-readable text for logging.
 */
struct NetError : std::runtime_error {
	NetError(NetErrc code, long http_status, std::string message)
	    : std::runtime_error(std::move(message))
	    , code(code)
	    , http_status(http_status) {}

	/// Coarse classification of the failure.
	NetErrc code;
	/// HTTP status when @ref code is @ref NetErrc::http_error, otherwise 0.
	long http_status = 0;
};

} // namespace aniparse
