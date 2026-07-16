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

/// An inline style over a run of text. Both the shared HTML/Markdown converters and a
/// parser building its own dialect (BBCode, DText) normalise into this closed vocabulary,
/// so a consumer renders one model regardless of the source's markup.
struct TextStyle {
	enum class Kind {
		Bold,
		Italic,
		Strikethrough,
		Spoiler,
	};
	Kind kind;
};

struct TextAttributeInfo {
	int start;
	int end;
	std::variant<Hyperlink, TextStyle> data;
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
