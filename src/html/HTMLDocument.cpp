/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/HTMLDocument.hpp"
#include "aniparse/html/HTMLParser.hpp"

#include <cassert>
#include <lexbor/html/interfaces/document.h>
#include <lexbor/html/interfaces/element.h>

namespace aniparse::html {

HTMLDocument::HTMLDocument(lxb_html_document_t* document) : document_(document) {
	assert(document == nullptr || lxb_dom_interface_node(document)->type == LXB_DOM_NODE_TYPE_DOCUMENT);
}

HTMLDocument::HTMLDocument(HTMLDocument&& other) noexcept
    : document_(std::exchange(other.document_, nullptr)) {
}

HTMLDocument::~HTMLDocument() {
	if (document_) {
		lxb_html_document_clean(document_);
		lxb_html_document_destroy(document_);
	}
}

HTMLDocument& HTMLDocument::operator=(HTMLDocument&& other) noexcept {
	if (std::addressof(other) != this) {
		document_ = std::exchange(other.document_, nullptr);
	}
	return *this;
}

DOMElementView HTMLDocument::as_element() const {
	lxb_dom_node_t* node = lxb_dom_interface_node(document_)->first_child;
	if (node->type == LXB_DOM_NODE_TYPE_DOCUMENT_TYPE) {
		node = node->next;
	}
	return DOMElementView(lxb_dom_interface_element(node));
}

DOMElementView HTMLDocument::head() const {
	lxb_dom_element_t* head = lxb_dom_interface_element(lxb_html_document_head_element(document_));
	return DOMElementView(head);
}

DOMElementView HTMLDocument::body() const {
	lxb_dom_element_t* body = lxb_dom_interface_element(lxb_html_document_body_element(document_));
	return DOMElementView(body);
}

HTMLDocument parse_html(std::string_view text) {
	//lxb_html_document_t* document = lxb_html_document_create();
	//lxb_status_t status = lxb_html_document_parse(document, reinterpret_cast<const lxb_char_t*>(text.data()), text.length());
	//if (status != LXB_STATUS_OK) {

	//}

	HTMLParser parser;
	return parser.parse(text);
}

} // namespace aniparse::html