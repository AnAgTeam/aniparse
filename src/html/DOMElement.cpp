/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#include "aniparse/html/DOMElement.hpp"
#include "aniparse/html/DOMAttributes.hpp"
#include "aniparse/html/DOMNode.hpp"

#include <cassert>
#include <array>
#include <functional>
#include <algorithm>
#include <numeric>
#include <cctype>
#include <lexbor/dom/interfaces/element.h>

/// Reserved space in the stack for find methods
constexpr size_t reserved_stack_size = 48;

namespace aniparse::html {
	using FindAllPredicate = std::function<bool(const DOMElementView&, std::string_view)>;

	static FindAllPredicate get_attr_value_predicate(std::string_view name, bool ignore_class_whitespaces) {
		if (name == "class" && ignore_class_whitespaces) {
			return [](const DOMElementView& element, std::string_view value) {
				return element.contains_class(value);
			};
		}
		else if (name == "class") {
			return [](const DOMElementView& element, std::string_view value) {
				return element.class_name() == value;
			};
		}
		else if (name == "id") {
			return [](const DOMElementView& element, std::string_view value) {
				return element.id() == value;
			};
		}
		return [name](const DOMElementView& element, std::string_view value) {
			std::optional<DOMAttrView> attr = element.find_attr(name);
			return attr ? attr->value() == value : false;
		};
	}

	static std::vector<DOMElementView> find_all_elements_predicate(lxb_dom_element_t* element, const std::string_view value, const FindAllPredicate& predicate) {
		std::array<DOMElementView, reserved_stack_size> buffer_space = {};

		size_t i = 0;
		auto iter = DOMElementWalkIterator(element);
		auto end = DOMElementWalkIterator{};
		while (iter != end && i < buffer_space.size()) {
			const DOMElementView& inner_element = *iter++;
			if (predicate(inner_element, value)) {
				buffer_space[i++] = inner_element;
			}
		}

		if (iter == end) {
			return std::vector<DOMElementView>(
				std::begin(buffer_space),
				std::begin(buffer_space) + i);
		}

		std::vector<DOMElementView> out(
			std::begin(buffer_space),
			std::begin(buffer_space) + i);
		while (iter != end) {
			const DOMElementView& element = *iter++;
			if (predicate(element, value)) {
				out.push_back(element);
			}
		}

		return out;
	}

	DOMElementView::DOMElementView(lxb_dom_element_t* element) : element_(element) {
		assert(element->node.type == LXB_DOM_NODE_TYPE_ELEMENT);
	}

	DOMElementView::operator bool() const {
		return element_ != nullptr;
	}

	DOMElementAttrsView DOMElementView::attributes() const {
		return DOMElementAttrsView(element_);
	}

	DOMElementIterator DOMElementView::begin() {
		return DOMElementIterator(element_);
	}

	DOMElementIterator DOMElementView::end() {
		return DOMElementIterator{};
	}

	lxb_dom_element_t* DOMElementView::get() const {
		return element_;
	}

	/// actually might have side effect as some cached allocations
	std::string_view DOMElementView::tag_name() const {
		size_t size;
		const lxb_char_t* name = lxb_dom_element_tag_name(element_, &size);
		return std::string_view(reinterpret_cast<const char*>(name), size);
	}

	/// actually might have side effect as some cached allocations
	std::string_view DOMElementView::class_name() const {
		size_t size;
		const lxb_char_t* name = lxb_dom_element_class(element_, &size);
		return std::string_view(reinterpret_cast<const char*>(name), size);
	}

	/// actually might have side effect as some cached allocations
	std::string_view DOMElementView::id() const {
		size_t size;
		const lxb_char_t* name = lxb_dom_element_id(element_, &size);
		return std::string_view(reinterpret_cast<const char*>(name), size);
	}

	/// actually might have side effect as some cached allocations
	std::optional<DOMElementView> DOMElementView::find(std::string_view tag) const {
		auto iter = std::find_if(DOMElementWalkIterator(element_), DOMElementWalkIterator{}, [&tag](const DOMElementView& element) {
			return element.tag_name() == tag;
		});
		if (iter == DOMElementWalkIterator{}) {
			return std::nullopt;
		}
		return *iter;
	}

	/// actually might have side effect as some cached allocations
	std::optional<DOMElementView> DOMElementView::find(std::string_view attr, std::string_view value, bool ignore_class_whitespaces) const {
		auto predicate = get_attr_value_predicate(attr, ignore_class_whitespaces);

		auto iter = std::find_if(DOMElementWalkIterator(element_), DOMElementWalkIterator{}, [&predicate, &value](const DOMElementView& element) {
			return predicate(element, value);
		});
		if (iter == DOMElementWalkIterator{}) {
			return std::nullopt;
		}
		return *iter;
	}

	/// actually might have side effect as some cached allocations
	std::vector<DOMElementView> DOMElementView::find_all(std::string_view tag) const {
		FindAllPredicate predicate = [](const DOMElementView& element, std::string_view value) {
			return element.tag_name() == value;
		};

		return find_all_elements_predicate(element_, tag, predicate);
	}

	/// actually might have side effect as some cached allocations
	std::vector<DOMElementView> DOMElementView::find_all(std::string_view attr, std::string_view value, bool ignore_class_whitespaces) const {
		auto predicate = get_attr_value_predicate(attr, ignore_class_whitespaces);

		return find_all_elements_predicate(element_, value, predicate);
	}

	/// actually might have side effect as some cached allocations
	bool DOMElementView::contains_class(std::string_view name) const {
		std::string_view full_class = class_name();
		for (size_t i = 0; i < full_class.size();) {
			size_t next_delim = full_class.find_first_of(" \t\n\f\r", i);
			if (full_class.compare(i, next_delim - i, name) == 0) {
				return true;
			}
			if (next_delim == std::string_view::npos) {
				return false;
			}
			i = next_delim + 1;
		}
		return false;
	}

	/// actually might have side effect as some cached allocations
	std::optional<DOMAttrView> DOMElementView::find_attr(std::string_view name) const {
		auto attrs = attributes();
		auto iter = std::find_if(std::begin(attrs), std::end(attrs), [&name](const DOMAttrView& attr) {
			return attr.name() == name;
		});
		if (iter == std::end(attrs)) {
			return std::nullopt;
		}
		return *iter;
	}

	/// actually might have side effect as some cached allocations
	std::optional<std::string_view> DOMElementView::get_attr(std::string_view name) const {
		auto attr = find_attr(name);
		if (!attr) {
			return std::nullopt;
		}
		return attr->value();
	}

	/// ! actually have side effect as cached string allocation
	std::string_view DOMElementView::content_text() const {
		size_t length;
		const lxb_char_t* text = lxb_dom_node_text_content(lxb_dom_interface_node(element_), &length);
		return std::string_view(reinterpret_cast<const char*>(text), length);
	}

	std::string DOMElementView::text() const {
		static auto remove_leading_trailing = [](std::string_view text) -> std::string_view {
			size_t off = text.find_first_not_of("\r\n\t\f ");
			size_t off_end = text.find_last_not_of("\r\n\t\f ");
			if (off >= off_end) {
				return "";
			}
			if (off_end == std::string_view::npos) {
				return text.substr(off);
			}
			return text.substr(off, off_end - off + 1);
		};
		static auto is_not_escape = [](lxb_char_t c) {
			return !std::isspace(c)
				|| std::isblank(c);
		};

		size_t output_length = std::accumulate(DOMNodeWalkIterator(*this), DOMNodeWalkIterator{}, 0ULL, [](size_t size, const DOMNodeView& node) {
			if (node.is_element()) {
				size += node.as_element().tag_name() == "BR" ? 1 : 0;
				return size;
			}
			if (!node.is_text()) {
				return size;
			}
			std::string_view node_text = remove_leading_trailing(node.as_text());
			size += std::count_if(std::begin(node_text), std::end(node_text), is_not_escape);
			return size;
		});

		std::string output_string(output_length, 0);
		auto output_iterator = std::begin(output_string);
		for (const DOMNodeView& node : DOMNodeWalkIterator(*this)) {
			if (node.is_element() && node.as_element().tag_name() == "BR") {
				*output_iterator++ = '\n';
				continue;
			}
			if (!node.is_text()) {
				continue;
			}
			std::string_view node_text = remove_leading_trailing(node.as_text());
			output_iterator = std::copy_if(
				std::begin(node_text),
				std::end(node_text),
				output_iterator,
				is_not_escape);
		}

		return output_string;
	}

	DOMElementWalkIterator::DOMElementWalkIterator(lxb_dom_element_t* element) : root_(lxb_dom_interface_node(element)), node_(root_ ? root_->first_child : nullptr) {
		walk_until_element();
	}

	DOMElementWalkIterator::DOMElementWalkIterator(const DOMElementView& element) : root_(lxb_dom_interface_node(element.element_)), node_(root_ ? root_->first_child : nullptr) {
		walk_until_element();
	}

	const DOMElementView& DOMElementWalkIterator::operator*() const {
		return node_view_;
	}

	const DOMElementView* DOMElementWalkIterator::operator->() const {
		return &node_view_;
	}

	DOMElementWalkIterator& DOMElementWalkIterator::operator++() {
		if (node_ == nullptr) {
			return *this;
		}

		do {
			next();
		} while (node_ != nullptr && node_->type != LXB_DOM_NODE_TYPE_ELEMENT);

		node_view_.element_ = lxb_dom_interface_element(node_);
		return *this;
	}

	DOMElementWalkIterator DOMElementWalkIterator::operator++(int) {
		DOMElementWalkIterator iter(*this);
		operator++();
		return iter;
	}

	void DOMElementWalkIterator::next() {
		if (node_->first_child != nullptr) {
			node_ = node_->first_child;
		}
		else {
			while (node_ != nullptr && node_->next == nullptr && node_ != root_) {
				node_ = node_->parent;
			}

			if (node_ != root_ && node_ != nullptr) {
				node_ = node_->next;
			}
		}

		if (node_ == root_) {
			// end
			node_ = nullptr;
		}
	}

	void DOMElementWalkIterator::walk_until_element() {
		while (node_ != nullptr && node_ != root_ && node_->type != LXB_DOM_NODE_TYPE_ELEMENT) {
			next();
		}
		node_view_.element_ = lxb_dom_interface_element(node_);
	}

	bool operator==(const DOMElementWalkIterator& left, const DOMElementWalkIterator& right) noexcept {
		return left.node_ == right.node_;
	}

	DOMElementIterator::DOMElementIterator(lxb_dom_element_t* element) : node_(element ? lxb_dom_interface_node(element)->first_child : nullptr) {
		iterate_until_element();
	}

	const DOMElementView& DOMElementIterator::operator*() const {
		return node_view_;
	}

	const DOMElementView* DOMElementIterator::operator->() const {
		return &node_view_;
	}

	DOMElementIterator& DOMElementIterator::operator--() {
		if (node_ != nullptr) {
			do {
				node_ = node_->prev;
			} while (node_ != nullptr && node_->type != LXB_DOM_NODE_TYPE_ELEMENT);
		}
		node_view_.element_ = lxb_dom_interface_element(node_);
		return *this;
	}

	DOMElementIterator DOMElementIterator::operator--(int) {
		DOMElementIterator iter(*this);
		operator--();
		return iter;
	}

	DOMElementIterator& DOMElementIterator::operator++() {
		if (node_ != nullptr) {
			node_ = node_->next;
			iterate_until_element();
		}
		return *this;
	}

	DOMElementIterator DOMElementIterator::operator++(int) {
		DOMElementIterator iter(*this);
		operator++();
		return iter;
	}

	void DOMElementIterator::iterate_until_element() {
		while (node_ != nullptr && node_->type != LXB_DOM_NODE_TYPE_ELEMENT) {
			node_ = node_->next;
		}
		node_view_.element_ = lxb_dom_interface_element(node_);
	}

	bool operator==(const DOMElementIterator& left, const DOMElementIterator& right) {
		return left.node_ == right.node_;
	}

	DOMElement::DOMElement(lxb_dom_element_t* element) : element_(element) {

	}
}