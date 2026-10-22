/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#include "aniparse/media/Publication.hpp"

namespace aniparse {
DefaultAiredStatuses AiredStatus::to_enum() const {
	if (name == aired_status_released) {
		return DefaultAiredStatuses::Released;
	}
	if (name == aired_status_ongoing) {
		return DefaultAiredStatuses::Ongoing;
	}
	if (name == aired_status_announced) {
		return DefaultAiredStatuses::Announced;
	}
	return DefaultAiredStatuses::Other;
}

} // namespace aniparse
