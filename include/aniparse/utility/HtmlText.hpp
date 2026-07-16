/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Text.hpp"
#include <string_view>

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
} // namespace aniparse::text
