/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#include "aniparse/anime/Release.hpp"

namespace aniparse {
	OptionalRequest<std::string> AsyncReleaseGetter::title() const {
		return ClientParsedRequest<std::string> {
			.request = GetRequest {
				.url = "https://www.google.com"
			},
			.parse = [](asyncnet::Response response) {
				return std::to_string(response.get_status_code());
			}
		};
		//return "some title";
	}
}
