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
		} else if (tag == "p" || tag == "div" || tag == "li") {
			break_line(out.text);
			walk(child, out);
			break_line(out.text);
		} else if (tag == "a") {
			const int start = static_cast<int>(out.text.size());
			walk(child, out);
			const int end = static_cast<int>(out.text.size());
			if (const auto href = element.get_attr("href"); href && !href->empty() && end > start)
				out.attributes.push_back({ start, end, Hyperlink{ std::string(*href) } });
		} else if (const auto kind = style_of(tag)) {
			const int start = static_cast<int>(out.text.size());
			walk(child, out);
			const int end = static_cast<int>(out.text.size());
			if (end > start)
				out.attributes.push_back({ start, end, TextStyle{ *kind } });
		} else {
			// Unknown/transparent tag (span, wbr, ...): keep its text, drop the tag.
			walk(child, out);
		}
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
