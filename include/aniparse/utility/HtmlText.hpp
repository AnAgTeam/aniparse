/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Text.hpp"
#include <string_view>

namespace aniparse::html {
class DOMElementView;
}

namespace aniparse::text {
/**
 * @brief Convert an HTML fragment into normalised AttributedText.
 *
 * A shared converter into the library's one text model: text nodes become the plain
 * @c text, inline tags become style/link runs (`<i>/<em>`, `<b>/<strong>`,
 * `<s>/<del>`, `<a href>`), `<br>` and block tags become line breaks, and anything
 * unrecognised is dropped while keeping its text. A parser whose source emits HTML
 * (or HTML-ish descriptions) routes them through here instead of shipping raw markup;
 * a parser with its own dialect (BBCode, DText) builds AttributedText by hand.
 *
 * Never throws: on a parse failure the raw string is returned as plain text.
 *
 * @param html The HTML fragment.
 * @return AttributedText with UTF-8 byte-offset runs into @c text.
 */
AttributedText from_html(std::string_view html);

/**
 * @brief Convert an already-parsed element's contents into normalised AttributedText.
 *
 * This is the DOM counterpart of @ref from_html(std::string_view). It walks the
 * element's child nodes directly, preserving the same inline styles, links, colours,
 * and block breaks without serialising the fragment and parsing it a second time.
 * The element and its owning HTMLDocument only need to outlive this call; the returned
 * text and attributes own all retained data.
 *
 * @param element Parsed element whose child nodes form the HTML fragment.
 * @return AttributedText with UTF-8 byte-offset runs into @c text.
 */
AttributedText from_html(const html::DOMElementView& element);
} // namespace aniparse::text
