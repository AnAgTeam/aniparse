/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/anime/Release.hpp"

namespace aniparse {
NetworkRequestTask<std::string> AsyncReleaseGetter::title(RequestorContext context) {
	auto response = co_await context.request(GetRequest{
	    .url = "https://www.google.com"});

	co_return std::to_string(response.status_code);
}
} // namespace aniparse
