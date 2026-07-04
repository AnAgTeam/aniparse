/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/HTMLParser.hpp"

#include <lexbor/html/parser.h>
#include <lexbor/html/interfaces/document.h>

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

	// From here the document is ours to own; wrap it now so every early return
	// below frees it instead of leaking.
	HTMLDocument parsed(document);

	if (!lxb_dom_interface_document(document)->doctype) {
		return unexpected(HTMLParseError("Invalid DOCTYPE for HTML"));
	}

	// The first child is doctype, probably
	lxb_dom_node_t* node = lxb_dom_interface_node(document);
	if (!node->first_child || !node->first_child->next || node->first_child->next->type != LXB_DOM_NODE_TYPE_ELEMENT) {
		return unexpected(HTMLParseError("Missing <HTML> tag for document"));
	}
	if (!document->body || !document->head) {
		return unexpected(HTMLParseError("Missing <HEAD> or <BODY> tag for document"));
	}

	return parsed;
}

HTMLDocument HTMLParser::parse(std::string_view text, bool remove_bom) {
	expected<HTMLDocument, HTMLParseError> result = try_parse(text, remove_bom);
	if (!result) {
		throw result.error();
	}
	return std::move(*result);
}
} // namespace aniparse::html