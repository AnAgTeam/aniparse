/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Headers.hpp"
#include "aniparse/utility/Expected.hpp"

#include <asyncnet/CancellingTask.hpp>

#include <optional>
#include <string>
#include <utility>

namespace aniparse {

/**
 * @brief Backend-neutral HTTP response handed to parser parse functors.
 * Filled by a ClientContext backend (curl, NSURLSession, ...). Carries no
 * transport handle, so it is a plain movable value independent of the backend
 * that produced it — a parser never sees curlpp/curl types.
 */
struct ResponseData {
	/// HTTP status code (e.g. 200, 404). 0 if the transport never got a status line.
	long status_code = 0;

	/// Response body. Empty if the request streamed into a caller-provided sink.
	std::string body;

	/// Response headers (case-insensitive). May be empty if the backend does not surface them.
	Headers headers;

	/// Final URL after redirects, for relative-link resolution. Empty if unknown.
	std::optional<std::string> effective_url;
};

enum class RequestErrorCode {
	Unknown,
	NotImplemented,
	InvalidArguments,
	ServerError,
	InvalidCredentials,
	// The response was fetched fine but its structure was not what the parser
	// expected (missing load-bearing element, changed markup, unparsable body).
	UnexpectedResponse,
};

struct RequestError {
	RequestErrorCode code;
	std::string message;
};

template<typename T, typename Error = RequestError>
using Response = expected<T, Error>;

inline auto make_response_error(RequestErrorCode code, std::string message) {
	return unexpected(RequestError{
		.code = code,
		.message = std::move(message)
	});
}

template<typename T>
using NetworkRequestTask = asyncnet::NetworkTask<Response<T>>;
} // namespace aniparse
