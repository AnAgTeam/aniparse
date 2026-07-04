/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/Expected.hpp"

#include <asyncnet/CancellingTask.hpp>

#include <string>
#include <utility>

namespace aniparse {
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
