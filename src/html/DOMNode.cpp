/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/DOMNode.hpp"
#include "aniparse/html/DOMElement.hpp"

#include <stdexcept>
#include <cassert>
#include <lexbor/dom/interfaces/node.h>

namespace aniparse::html {
	DOMNodeView::DOMNodeView(lxb_dom_node_t* node) noexcept : node_(node) {
	}

	DOMNodeView::DOMNodeView(const DOMElementView& element) noexcept : DOMNodeView(lxb_dom_interface_node(element.element_)) {
	}

	DOMNodeView::operator bool() const noexcept {
		return node_ != nullptr;
	}

	lxb_dom_node_t* DOMNodeView::get() const noexcept {
		return node_;
	}

	[[nodiscard]] bool DOMNodeView::is_element() const noexcept {
		return node_->type == LXB_DOM_NODE_TYPE_ELEMENT;
	}

	[[nodiscard]] bool DOMNodeView::is_text() const noexcept {
		return node_->type == LXB_DOM_NODE_TYPE_TEXT;
	}

	DOMElementView DOMNodeView::as_element() const {
		if (!is_element()) {
			throw std::runtime_error("DOM node isn't an element");
		}
		return lxb_dom_interface_element(node_);
	}

	std::string_view DOMNodeView::as_text() const {
		if (!is_text()) {
			throw std::runtime_error("DOM node isn't a text");
		}

		size_t length;
		const lxb_char_t* text = lxb_dom_node_text_content(node_, &length);
		return std::string_view(reinterpret_cast<const char*>(text), length);
	}

	DOMNodeView DOMNodeView::first_child() const noexcept {
		return node_->first_child;
	}

	DOMNodeView DOMNodeView::parent() const noexcept {
		return node_->parent;
	}


	DOMNodeView DOMNodeView::prev() const noexcept {
		return node_->prev;
	}

	DOMNodeView DOMNodeView::next() const noexcept {
		return node_->next;
	}

	bool aniparse::html::operator==(const DOMNodeView& left, const DOMNodeView& right) noexcept {
		return left.node_ == right.node_;
	}

	DOMNodeWalkIterator::DOMNodeWalkIterator(lxb_dom_node_t* node) noexcept
		: root_(lxb_dom_interface_node(node)), node_(root_ ? root_.first_child() : nullptr) {

	}

	DOMNodeWalkIterator::DOMNodeWalkIterator(DOMNodeView node) noexcept : DOMNodeWalkIterator(node.node_) {
	}

	const DOMNodeView& DOMNodeWalkIterator::operator*() const noexcept {
		return node_;
	}

	const DOMNodeView* DOMNodeWalkIterator::operator->() const noexcept {
		return &node_;
	}

	DOMNodeWalkIterator& DOMNodeWalkIterator::operator++() noexcept {
		if (node_) {
			next();
		}
		return *this;
	}

	DOMNodeWalkIterator DOMNodeWalkIterator::operator++(int) noexcept {
		DOMNodeWalkIterator iter(*this);
		operator++();
		return iter;
	}

	void DOMNodeWalkIterator::next() {
		if (node_.first_child()) {
			node_ = node_.first_child();
			return;
		}

		while (node_ && !node_.next() && node_ != root_) {
			node_ = node_.parent();
		}

		if (node_ == root_) {
			// end
			node_ = nullptr;
		}

		if (node_ && node_.next()) {
			node_ = node_.next();
		}
	}

	bool operator==(const DOMNodeWalkIterator& left, const DOMNodeWalkIterator& right) noexcept {
		return left.node_ == right.node_;
	}
}