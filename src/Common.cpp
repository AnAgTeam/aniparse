/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Common.hpp"

namespace aniparse {
	Series Series::make_original(std::string referer) {
		// TODO: multilang
		return Series{
			.name = "Original",
			.referer = std::move(referer)
		};
	}

	AiredStatus AiredStatus::make_default(DefaultAiredStatuses status, std::chrono::system_clock::time_point time) {
		// TODO: multilang
		switch (status) {
		case DefaultAiredStatuses::Released:
			return AiredStatus{
				.name = "Released",
				.time = time
			};
		case DefaultAiredStatuses::Ongoing:
			return AiredStatus{
				.name = "Ongoing",
				.time = time
			};
		case DefaultAiredStatuses::Announced:
			return AiredStatus{
				.name = "Announced",
				.time = time
			};
		}
	}
}