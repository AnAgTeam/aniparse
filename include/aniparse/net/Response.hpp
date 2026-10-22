/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/Headers.hpp"
#include <optional>
#include <string>

namespace aniparse {

/**
 * @brief Backend-neutral HTTP response handed to parser parse functors.
 * Filled by a HttpClient backend (curl, NSURLSession, ...). Carries no
 * transport handle, so it is a plain movable value independent of the backend
 * that produced it — a parser never sees curlpp/curl types.
 */
struct HttpResponse {
	/// HTTP status code (e.g. 200, 404). 0 if the transport never got a status line.
	long status_code = 0;

	/// Response body. Empty if the request streamed into a caller-provided sink.
	std::string body;

	/// Response headers (case-insensitive). May be empty if the backend does not surface them.
	Headers headers;

	/// Final URL after redirects, for relative-link resolution. Empty if unknown.
	std::optional<std::string> effective_url;
};

} // namespace aniparse
