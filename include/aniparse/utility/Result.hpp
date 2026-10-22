/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/Expected.hpp"

#include "aniparse/net/CancellingTask.hpp"

#include <optional>
#include <string>
#include <utility>

namespace aniparse {

/**
 * @brief The kind of failure a request ended in — the coarse, consumer-facing
 * classification an app switches on (retry, re-authenticate, show "not found").
 * Deliberately small: the exact HTTP status and the human-readable detail travel
 * alongside it in @ref RequestError, so new failure shapes do not need new
 * enumerators.
 */
enum class RequestErrorCode {
	/// Unclassified failure — including a non-2xx status that maps to none of the
	/// cases below, and a source reporting an error inside an otherwise-fine response.
	Unknown,
	/// The operation is not available: the parser does not implement this getter
	/// capability, or the transport backend cannot express the requested HTTP verb.
	/// Not a transient failure — retrying cannot help.
	NotImplemented,
	/// The caller's arguments are unusable: a filter or query the source cannot serve,
	/// or a missing credential. Caught before or instead of a request.
	InvalidArguments,
	/// Transport failure — timeout, no connection, DNS, TLS. No HTTP status was reached.
	NetworkError,
	/// The request was cancelled on purpose (a stop was requested), not a failure of
	/// the source. Distinguished from NetworkError so a cancelled operation is not
	/// reported to the user as an error.
	Cancelled,
	/// The source answered with a 5xx.
	ServerError,
	/// The source rejected the credentials or the caller's access to the resource
	/// (401 / 403) — the signal to re-authenticate.
	InvalidCredentials,
	/// The requested resource does not exist (404), or the source answered with an
	/// empty/absent record where an item was expected.
	NotFound,
	/// The source is throttling the client (429).
	RateLimited,
	/// The response was fetched fine but its structure was not what the parser
	/// expected (missing load-bearing element, changed markup, unparsable body).
	/// The characteristic symptom of a source that changed its layout.
	UnexpectedResponse,
	/// The operation cannot complete without an interactive step the library
	/// cannot perform headlessly — a captcha, a social-OAuth or in-browser login,
	/// a 2FA prompt. The caller chooses the sign-in flow up front from
	/// ParserProfile::auth_info and collects the credential accordingly; this is the
	/// escalation signal for when a *headless* attempt (authenticate with a
	/// username/password) turns out to need that interaction after all, telling the
	/// caller to fall back to the interactive flow auth_info describes. Distinct
	/// from @ref RequestErrorCode::InvalidCredentials (a credential was present
	/// and rejected): here
	/// there is nothing to reject yet.
	InteractionRequired,
};

/**
 * @brief The error half of every Result — what went wrong, in one value.
 * Recoverable failures travel through this channel rather than as exceptions, so
 * a getter's failure path is part of its signature.
 */
struct RequestError {
	/// Which kind of failure this is — the consumer-facing part of the error,
	/// and what the app switches on.
	RequestErrorCode code;
	/// Human-readable detail for logs and diagnostics; not for switching on.
	std::string message;
	/// Exact HTTP status when the error is status-based; nullopt otherwise.
	std::optional<long> http_status;
	/// Diagnostic only: full response body (moved in) for logging or reading a
	/// server-sent error payload (e.g. an API's {"error": ...} on a 4xx). Not
	/// part of the consumer-facing contract — the app switches on @ref RequestError::code, not
	/// on this. Candidate for removal from the public error if it proves unused.
	std::string body;
};

/**
 * @brief The result of an operation that talks to a source: the value on success,
 * a @ref RequestError on failure. The return type of every parser getter — a
 * caller must inspect it before reading the value, and a failure cannot be
 * ignored by accident.
 * @tparam T The success value type.
 * @tparam Error The failure type; @ref RequestError unless an operation needs a
 *         richer one.
 */
template<typename T, typename Error = RequestError>
using Result = expected<T, Error>;

/**
 * @brief Build the failure half of a @ref Result in one call — the standard way
 * a getter reports an error, keeping the RequestError fields named at the site
 * that knows them.
 * @param code Failure classification
 * @param message Human-readable detail for logs; not for switching on
 * @param http_status Exact HTTP status, when the failure is status-based
 * @param body Response body, for diagnostics; moved in
 * @return An unexpected value assignable to any Result<T>
 */
inline auto make_response_error(RequestErrorCode code, std::string message,
                                std::optional<long> http_status = std::nullopt,
                                std::string body = {}) {
	return unexpected(RequestError{
		.code        = code,
		.message     = std::move(message),
		.http_status = http_status,
		.body        = std::move(body),
	});
}

/**
 * @brief The return type of every asynchronous, cancellable operation that may hit
 * the network: an awaitable task yielding a @ref Result.
 * @tparam T The success value type the task yields.
 * @note A task carrying no value uses a placeholder success type; T = void leaves
 *       the coroutine promise unset.
 */
template<typename T>
using RequestTask = asyncnet::NetworkTask<Result<T>>;
} // namespace aniparse
