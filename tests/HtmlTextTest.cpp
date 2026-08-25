/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/html/CompiledSelector.hpp>
#include <aniparse/html/HTMLParser.hpp>
#include <aniparse/utility/HtmlText.hpp>

#include <string>
#include <variant>

using aniparse::AttributedText;
using aniparse::Hyperlink;
using aniparse::TextColor;
using aniparse::TextStyle;
using aniparse::text::from_html;

namespace {
// The text a run covers, so a test asserts on the span, not on raw offsets.
std::string covered(const AttributedText& t, const aniparse::TextAttributeInfo& a) {
	return t.text.substr(static_cast<std::size_t>(a.start), static_cast<std::size_t>(a.end - a.start));
}
} // namespace

TEST_CASE("from_html drops tags and keeps text", "[htmltext]") {
	const AttributedText r = from_html("<p>Hello world</p>");
	REQUIRE(r.text == "Hello world");
	REQUIRE(r.attributes.empty());
}

TEST_CASE("from_html maps inline styles to runs", "[htmltext]") {
	SECTION("italic") {
		const AttributedText r = from_html("a <i>bee</i> c");
		REQUIRE(r.text == "a bee c");
		REQUIRE(r.attributes.size() == 1);
		REQUIRE(covered(r, r.attributes[0]) == "bee");
		const auto* style = std::get_if<TextStyle>(&r.attributes[0].data);
		REQUIRE(style != nullptr);
		REQUIRE(style->kind == TextStyle::Kind::Italic);
	}
	SECTION("em is italic too") {
		const AttributedText r = from_html("<em>x</em>");
		REQUIRE(r.attributes.size() == 1);
		REQUIRE(std::get<TextStyle>(r.attributes[0].data).kind == TextStyle::Kind::Italic);
	}
	SECTION("bold") {
		const AttributedText r = from_html("a <b>bee</b> c");
		REQUIRE(r.text == "a bee c");
		REQUIRE(std::get<TextStyle>(r.attributes.at(0).data).kind == TextStyle::Kind::Bold);
	}
	SECTION("strikethrough") {
		const AttributedText r = from_html("<s>gone</s>");
		REQUIRE(std::get<TextStyle>(r.attributes.at(0).data).kind == TextStyle::Kind::Strikethrough);
	}
}

TEST_CASE("from_html carries link href", "[htmltext]") {
	const AttributedText r = from_html(R"(see <a href="https://x.test/a">here</a>.)");
	REQUIRE(r.text == "see here.");
	REQUIRE(r.attributes.size() == 1);
	REQUIRE(covered(r, r.attributes[0]) == "here");
	const auto* link = std::get_if<Hyperlink>(&r.attributes[0].data);
	REQUIRE(link != nullptr);
	REQUIRE(link->url == "https://x.test/a");
}

TEST_CASE("from_html turns br and blocks into line breaks", "[htmltext]") {
	SECTION("br") {
		const AttributedText r = from_html("a<br>b");
		REQUIRE(r.text == "a\nb");
	}
	SECTION("a run of breaks is capped at a blank line") {
		REQUIRE(from_html("a<br>b").text == "a\nb");
		REQUIRE(from_html("a<br><br>b").text == "a\n\nb");
		REQUIRE(from_html("a<br><br><br><br>b").text == "a\n\nb");
	}
	SECTION("list items break") {
		const AttributedText r = from_html("<ul><li>one</li><li>two</li></ul>");
		REQUIRE(r.text == "one\ntwo");
	}
}

TEST_CASE("from_html keeps text of unknown tags", "[htmltext]") {
	const AttributedText r = from_html(R"(<span class="x">kept</span>)");
	REQUIRE(r.text == "kept");
	REQUIRE(r.attributes.empty());
}

TEST_CASE("from_html carries foreground colours", "[htmltext]") {
	const AttributedText r = from_html(R"(<span style="font-weight: 400; color: #789">dim</span> <font color="#10203080">faint</font>)");
	REQUIRE(r.text == "dim faint");
	bool dim = false, faint = false;
	for (const auto& attribute : r.attributes) {
		const auto* color = std::get_if<TextColor>(&attribute.data);
		if (!color) continue;
		if (covered(r, attribute) == "dim")
			dim = color->red == 0x77 && color->green == 0x88 && color->blue == 0x99 && color->alpha == 0xff;
		if (covered(r, attribute) == "faint")
			faint = color->red == 0x10 && color->green == 0x20 && color->blue == 0x30 && color->alpha == 0x80;
	}
	REQUIRE(dim);
	REQUIRE(faint);
}

TEST_CASE("from_html on a real AniList-shaped description", "[htmltext]") {
	// The exact shape AniList returns with asHtml:true: block <p>, <br> runs, a source
	// attribution, an italic label, and a <ul>/<li> list.
	constexpr std::string_view input =
	    "<p>The name says it all! Denji's life of poverty.<br><br>"
	    "(Source: MANGA Plus)<br><br><i>Notes:</i></p><ul><li>Nominated for an award</li></ul>";
	const AttributedText r = from_html(input);

	// No angle brackets survive: every tag became text or a break.
	REQUIRE(r.text.find('<') == std::string::npos);
	REQUIRE(r.text.find("<br>") == std::string::npos);
	REQUIRE(r.text.find("The name says it all!") != std::string::npos);
	REQUIRE(r.text.find("(Source: MANGA Plus)") != std::string::npos);
	REQUIRE(r.text.find("Nominated for an award") != std::string::npos);

	// "Notes:" is italic.
	bool notes_italic = false;
	for (const auto& a : r.attributes)
		if (covered(r, a) == "Notes:")
			if (const auto* s = std::get_if<TextStyle>(&a.data); s && s->kind == TextStyle::Kind::Italic)
				notes_italic = true;
	REQUIRE(notes_italic);
}

TEST_CASE("from_html converts a parsed element without re-parsing its contents", "[htmltext]") {
	aniparse::html::HTMLParser parser;
	auto document = parser.try_parse(R"(<!doctype html><html><body><div id="description">A <i>styled</i> <a href="https://x.test">link</a></div></body></html>)");
	REQUIRE(document.has_value());
	auto description = document->query(aniparse::html::SelectorCompiler{}.compile("#description"));
	REQUIRE(description.has_value());

	const AttributedText r = from_html(*description);
	REQUIRE(r.text == "A styled link");
	REQUIRE(r.attributes.size() == 2);
	CHECK(covered(r, r.attributes[0]) == "styled");
	CHECK(covered(r, r.attributes[1]) == "link");
}

TEST_CASE("from_html never throws on malformed input", "[htmltext]") {
	REQUIRE_NOTHROW(from_html("<p><i>unclosed <b> mix"));
	REQUIRE_NOTHROW(from_html(""));
	REQUIRE_NOTHROW(from_html("<<<>>>"));
}
