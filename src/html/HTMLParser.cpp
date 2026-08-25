/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/HTMLParser.hpp"

#include <lexbor/html/parser.h>

namespace aniparse::html {
HTMLParser::HTMLParser() : parser_(lxb_html_parser_create()) {
	lxb_html_parser_init(parser_);
}

HTMLParser::HTMLParser(HTMLParser&& other) noexcept : parser_(std::exchange(other.parser_, nullptr)) {
}

HTMLParser::~HTMLParser() {
	if (parser_) {
		lxb_html_parser_clean(parser_);
		lxb_html_parser_destroy(parser_);
	}
}

HTMLParser& HTMLParser::operator=(HTMLParser&& other) noexcept {
	if (std::addressof(other) != this) {
		parser_ = std::exchange(other.parser_, nullptr);
	}
	return *this;
}

expected<HTMLDocument, HTMLParseError> HTMLParser::try_parse(std::string_view text, bool remove_bom) {
	if (remove_bom && text.compare(0, 3, "\xEF\xBB\xBF") == 0) {
		text = text.substr(3);
	}

	lxb_html_document_t* document = lxb_html_parse(parser_, reinterpret_cast<const lxb_char_t*>(text.data()), text.size());
	if (!document || parser_->status != LXB_STATUS_OK) {
		if (document) {
			lxb_html_document_clean(document);
			lxb_html_document_destroy(document);
		}
		return unexpected(HTMLParseError("Failed to parse HTML"));
	}

	// Lexbor follows HTML's document-recovery rules: a doctype and explicit
	// html/head/body elements are optional in source and are synthesized when
	// absent. Expose that browser-equivalent DOM to callers.
	return HTMLDocument(document);
}

HTMLDocument HTMLParser::parse(std::string_view text, bool remove_bom) {
	expected<HTMLDocument, HTMLParseError> result = try_parse(text, remove_bom);
	if (!result) {
		throw result.error();
	}
	return std::move(*result);
}
} // namespace aniparse::html
