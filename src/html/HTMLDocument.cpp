#include "aniparse/html/HTMLDocument.hpp"

#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <ranges>
#include <cassert>

namespace aniparse {

	HTMLDocument::HTMLDocument(lxb_html_document* document) : document_(document) {
		assert(document == nullptr || lxb_dom_interface_node(document)->type == LXB_DOM_NODE_TYPE_DOCUMENT);
	}

	HTMLDocument::HTMLDocument(HTMLDocument&& other) noexcept : document_(std::exchange(other.document_, nullptr)) {

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

	std::optional<DOMElementView> HTMLDocument::find_first_by_class(std::string_view name) const {
		lxb_dom_collection_t* collection = lxb_dom_collection_make(&document_->dom_document, 1);
		lxb_status_t status = lxb_dom_elements_by_class_name(lxb_dom_interface_element(document_), collection, reinterpret_cast<const lxb_char_t*>(name.data()), name.length());
		return DOMElementView(lxb_dom_interface_element(collection->array.list[0]));
	}

	//HTMLDocument::HTMLDocument(std::string_view text) : document_(lxb_html_document_create()) {
	//	lxb_html_parse
	//}


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

	HTMLDocument HTMLParser::parse(std::string_view text) {
		return HTMLDocument(lxb_html_parse(parser_, reinterpret_cast<const lxb_char_t*>(text.data()), text.size()));
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
	
	std::string_view DOMElementView::tag_name() const {
		size_t size;
		const lxb_char_t* name = lxb_dom_element_tag_name(element_, &size);
		return std::string_view(reinterpret_cast<const char*>(name), size);
	}
	
	std::string_view DOMElementView::class_name() const {
		size_t size;
		const lxb_char_t* name = lxb_dom_element_class(element_, &size);
		return std::string_view(reinterpret_cast<const char*>(name), size);
	}

	std::string_view DOMElementView::id() const {
		size_t size;
		const lxb_char_t* name = lxb_dom_element_id(element_, &size);
		return std::string_view(reinterpret_cast<const char*>(name), size);
	}

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

	std::optional<std::string_view> DOMElementView::get_attr(std::string_view name) const {
		auto attr = find_attr(name);
		if (!attr) {
			return std::nullopt;
		}
		return attr->value();
	}

	DOMCollection::DOMCollection(DOMCollection&& other) noexcept : collection_(std::exchange(other.collection_, nullptr)) {

	}

	DOMCollection::~DOMCollection() {
		if (collection_) {
			lxb_dom_collection_clean(collection_);
			lxb_dom_collection_destroy(collection_, false);
		}
	}

	DOMCollection& DOMCollection::operator=(DOMCollection&& other) noexcept {
		if (std::addressof(other) != this) {
			collection_ = std::exchange(other.collection_, nullptr);
		}
		return *this;
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

	void DOMElementWalkIterator::next() {
		if (node_->first_child != nullptr) {
			node_ = node_->first_child;
		}
		else {
			while (node_->next == nullptr && node_ != root_) {
				node_ = node_->parent;
			}

			if (node_ != nullptr) {
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

	DOMElement::DOMElement(lxb_dom_element_t* element) : element_(element) {

	}

	bool operator==(const DOMAttrsIterator& left, const DOMAttrsIterator& right) {
		return left.attr_.attr_ == right.attr_.attr_;
	}

	bool operator==(const DOMElementIterator& left, const DOMElementIterator& right) {
		return left.node_ == right.node_;
	}

	bool operator==(const DOMElementWalkIterator& left, const DOMElementWalkIterator& right) noexcept {
		return left.node_ == right.node_;
	}

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

	DOMAttrsIterator& DOMAttrsIterator::operator++() {
		attr_.attr_ = attr_.attr_->next;
		return *this;
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
		--iter;
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
		++iter;
		return iter;
	}

	void DOMElementIterator::iterate_until_element() {
		while (node_ != nullptr && node_->type != LXB_DOM_NODE_TYPE_ELEMENT) {
			node_ = node_->next;
		}
		node_view_.element_ = lxb_dom_interface_element(node_);
	}

}