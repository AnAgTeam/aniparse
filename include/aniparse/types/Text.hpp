/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <string>
#include <variant>
#include <vector>

namespace aniparse {
struct Hyperlink {
	std::string url;
};

struct TextAttributeInfo {
	int start;
	int end;
	std::variant<Hyperlink> data;
};

/**
 * Structure representing text with some attributes
 * in it like links, colors, etc.
 */
struct AttributedText {
	std::string text;
	std::vector<TextAttributeInfo> attributes;
};
} // namespace aniparse
