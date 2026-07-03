/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <functional>
#include <map>
#include <string>

namespace aniparse {
/**
 * @brief Data used to serialize/deserialize "units" from getters
 */
struct SerializedGetterData {
	std::string url;
	std::map<std::string, std::string, std::less<>> params;
};
} // namespace aniparse
