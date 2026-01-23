/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Common.hpp"

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

	DefaultUserLists UserList::to_enum() const {
		if (name == user_list_planning) {
			return DefaultUserLists::Planning;
		}
		if (name == user_list_dropped) {
			return DefaultUserLists::Dropped;
		}
		if (name == user_list_favorite) {
			return DefaultUserLists::Favorite;
		}
		if (name == user_list_watched) {
			return DefaultUserLists::Watched;
		}
		if (name == user_list_watching) {
			return DefaultUserLists::Watching;
		}
		if (name == user_list_reading) {
			return DefaultUserLists::Reading;
		}
		if (name == user_list_read) {
			return DefaultUserLists::Read;
		}
		return DefaultUserLists::Other;
	}
}