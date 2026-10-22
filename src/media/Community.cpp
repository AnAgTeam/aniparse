/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#include "aniparse/media/Community.hpp"

namespace aniparse {
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
	if (name == user_list_on_hold) {
		return DefaultUserLists::OnHold;
	}
	return DefaultUserLists::Other;
}
} // namespace aniparse
