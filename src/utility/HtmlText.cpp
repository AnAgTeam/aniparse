/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/utility/HtmlText.hpp"
#include "aniparse/html/DOMElement.hpp"
#include "aniparse/html/DOMNode.hpp"
#include "aniparse/html/HTMLDocument.hpp"
#include "aniparse/html/HTMLParser.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace aniparse::text {
namespace {
using html::DOMElementView;
using html::DOMNodeView;

std::optional<TextStyle::Kind> style_of(std::string_view tag) {
	if (tag == "i" || tag == "em")
		return TextStyle::Kind::Italic;
	if (tag == "b" || tag == "strong")
		return TextStyle::Kind::Bold;
	if (tag == "s" || tag == "del" || tag == "strike")
		return TextStyle::Kind::Strikethrough;
	return std::nullopt;
}

std::string_view trim(std::string_view value) {
	while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\n' || value.front() == '\r'))
		value.remove_prefix(1);
	while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\n' || value.back() == '\r'))
		value.remove_suffix(1);
	return value;
}

int hex_digit(char character) {
	if (character >= '0' && character <= '9') return character - '0';
	if (character >= 'a' && character <= 'f') return character - 'a' + 10;
	if (character >= 'A' && character <= 'F') return character - 'A' + 10;
	return -1;
}

std::optional<TextColor> color_value(std::string_view value) {
	value = trim(value);
	if (value.size() < 4 || value.front() != '#') return std::nullopt;
	value.remove_prefix(1);
	if (value.size() != 3 && value.size() != 4 && value.size() != 6 && value.size() != 8) return std::nullopt;
	auto component = [&](std::size_t offset) -> std::optional<std::uint8_t> {
		const int high = hex_digit(value[offset]);
		const int low = value.size() <= 4 ? high : hex_digit(value[offset + 1]);
		if (high < 0 || low < 0) return std::nullopt;
		return static_cast<std::uint8_t>(high * 16 + low);
	};
	const std::size_t step = value.size() <= 4 ? 1 : 2;
	const auto red = component(0), green = component(step), blue = component(step * 2);
	if (!red || !green || !blue) return std::nullopt;
	const auto alpha = value.size() == 4 || value.size() == 8 ? component(step * 3) : std::optional<std::uint8_t>{ 255 };
	if (!alpha) return std::nullopt;
	return TextColor{ *red, *green, *blue, *alpha };
}

std::optional<TextColor> color_of(const DOMElementView& element) {
	if (const auto value = element.get_attr("color"))
		if (const auto color = color_value(*value)) return color;
	if (const auto style = element.get_attr("style")) {
		std::string_view declarations = *style;
		while (!declarations.empty()) {
			const std::size_t separator = declarations.find(';');
			const std::string_view declaration = trim(declarations.substr(0, separator));
			declarations = separator == std::string_view::npos ? std::string_view{} : declarations.substr(separator + 1);
			const std::size_t colon = declaration.find(':');
			if (colon == std::string_view::npos || trim(declaration.substr(0, colon)) != "color") continue;
			if (const auto color = color_value(declaration.substr(colon + 1))) return color;
		}
	}
	return std::nullopt;
}

// Emit a line break only when the text does not already end in one, so successive block
// tags and <br>s collapse instead of stacking blank lines.
void break_line(std::string& text) {
	if (!text.empty() && text.back() != '\n')
		text += '\n';
}

void walk(DOMNodeView node, AttributedText& out) {
	for (DOMNodeView child = node.first_child(); child; child = child.next()) {
		if (child.is_text()) {
			out.text += child.as_text();
			continue;
		}
		if (!child.is_element())
			continue;

		DOMElementView element = child.as_element();
		// lexbor returns HTML tag names uppercased ("BR", "I"), unlike attribute names.
		std::string tag;
		for (const char c : element.tag_name())
			tag += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;

		if (tag == "br") {
			// A newline per <br>, but capped at a blank line so a run of <br>s (AniList
			// pads with them) does not open a chasm. Capping here keeps run offsets valid.
			const std::size_t n = out.text.size();
			if (n < 2 || out.text[n - 1] != '\n' || out.text[n - 2] != '\n')
				out.text += '\n';
			continue;
		}
		if (tag == "p" || tag == "div" || tag == "li") {
			break_line(out.text);
			walk(child, out);
			break_line(out.text);
			continue;
		}

		const int start = static_cast<int>(out.text.size());
		walk(child, out);
		const int end = static_cast<int>(out.text.size());
		if (end <= start) continue;
		if (tag == "a")
			if (const auto href = element.get_attr("href"); href && !href->empty())
				out.attributes.push_back({ start, end, Hyperlink{ std::string(*href) } });
		if (const auto kind = style_of(tag))
			out.attributes.push_back({ start, end, TextStyle{ *kind } });
		if (const auto color = color_of(element))
			out.attributes.push_back({ start, end, *color });
	}
}
} // namespace

AttributedText from_html(std::string_view html) {
	AttributedText out;
	try {
		// Descriptions arrive as bare fragments; lexbor's parser flags a document with no
		// doctype/html/body as malformed (try_parse fails), so wrap it in a minimal one.
		std::string document;
		document.reserve(html.size() + 32);
		document += "<!DOCTYPE html><html><body>";
		document += html;
		document += "</body></html>";

		html::HTMLParser parser;
		auto parsed = parser.try_parse(document);
		if (!parsed) {
			out.text = std::string(html);
			return out;
		}
		if (const DOMElementView body = parsed->body())
			walk(DOMNodeView(body), out);
	} catch (...) {
		out.text = std::string(html);
		out.attributes.clear();
		return out;
	}

	// Offset-safe tidy only: trailing whitespace does not move any run's start/end.
	// (Leading/interior collapsing would shift offsets, so it is left to the renderer.)
	while (!out.text.empty() && (out.text.back() == '\n' || out.text.back() == ' ' || out.text.back() == '\t'))
		out.text.pop_back();

	return out;
}
} // namespace aniparse::text
