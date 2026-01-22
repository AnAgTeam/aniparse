/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <type_traits>

namespace aniparse::concepts {
	template<typename T>
	concept hashable = requires (T a) {
		std::hash<T>{}(a);
	};
}