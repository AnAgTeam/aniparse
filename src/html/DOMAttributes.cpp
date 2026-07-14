/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/DOMAttributes.hpp"

#include <cassert>
#include <lexbor/dom/interfaces/attr.h>
#include <lexbor/dom/interfaces/element.h>

namespace aniparse::html {
DOMAttrView::DOMAttrView(lxb_dom_attr_t* attr) : attr_(attr) {
	assert(attr == nullptr || attr->node.type == LXB_DOM_NODE_TYPE_ATTRIBUTE);
}

std::string_view DOMAttrView::name() const {
	size_t length;
	const lxb_char_t* value = lxb_dom_attr_local_name(attr_, &length);
	return std::string_view(reinterpret_cast<const char*>(value), length);
}

std::string_view DOMAttrView::value() const {
	size_t length;
	const lxb_char_t* value = lxb_dom_attr_value(attr_, &length);
	return std::string_view(reinterpret_cast<const char*>(value), length);
}

DOMElementAttrsView::DOMElementAttrsView(lxb_dom_element_t* element) : element_(element) {
	assert(element == nullptr || element->node.type == LXB_DOM_NODE_TYPE_ELEMENT);
}

DOMAttrsIterator DOMElementAttrsView::begin() {
	// An invalid view (no element) iterates as empty rather than dereferencing null:
	// the search methods built on this — DOMElementView::find_attr / get_attr — are
	// total, and a step that found nothing hands on an invalid view for them to
	// query.
	if (element_ == nullptr) {
		return DOMAttrsIterator{};
	}
	return DOMAttrsIterator(element_->first_attr);
}

DOMAttrsIterator DOMElementAttrsView::end() {
	return DOMAttrsIterator{};
}

DOMAttrsIterator::DOMAttrsIterator(lxb_dom_attr_t* attr) : attr_(attr) {
}

const DOMAttrView& DOMAttrsIterator::operator*() const {
	return attr_;
}

const DOMAttrView* DOMAttrsIterator::operator->() const {
	return &attr_;
}

DOMAttrsIterator& DOMAttrsIterator::operator--() {
	attr_.attr_ = attr_.attr_->prev;
	return *this;
}

DOMAttrsIterator DOMAttrsIterator::operator--(int) {
	DOMAttrsIterator iter(*this);
	operator--();
	return iter;
}

DOMAttrsIterator& DOMAttrsIterator::operator++() {
	attr_.attr_ = attr_.attr_->next;
	return *this;
}

DOMAttrsIterator DOMAttrsIterator::operator++(int) {
	DOMAttrsIterator iter(*this);
	operator++();
	return iter;
}

bool operator==(const DOMAttrsIterator& left, const DOMAttrsIterator& right) {
	return left.attr_.attr_ == right.attr_.attr_;
}
} // namespace aniparse::html