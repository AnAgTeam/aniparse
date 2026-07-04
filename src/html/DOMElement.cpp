/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/DOMElement.hpp"
#include "aniparse/html/DOMAttributes.hpp"
#include "aniparse/html/DOMNode.hpp"
#include "aniparse/html/CompiledSelector.hpp"

#include <cassert>
#include <algorithm>
#include <cctype>
#include <lexbor/dom/interfaces/element.h>
#include <lexbor/dom/interfaces/document.h>
#include <lexbor/selectors/selectors.h>
#include <lexbor/tag/tag.h>

namespace aniparse::html {

/// Resolve a tag name to lexbor's numeric tag id via the owner document's tag
/// table, so tag matching is an integer compare instead of materializing each
/// element's (uppercased, cached-allocated) name and comparing strings. lexbor
/// interns tags at the document level, so the element reaches the table through
/// its owner document. The lookup lowercases its input, so the UPPERCASE names
/// parsers pass resolve fine and the id itself is case-independent. Unknown tags
/// and detached elements yield LXB_TAG__UNDEF, which no real element carries, so
/// nothing matches.
static lxb_tag_id_t resolve_tag_id(lxb_dom_element_t* element, std::string_view tag) {
	if (element == nullptr) {
		return LXB_TAG__UNDEF;
	}
	lxb_dom_document_t* document = lxb_dom_interface_node(element)->owner_document;
	if (document == nullptr) {
		return LXB_TAG__UNDEF;
	}
	return lxb_tag_id_by_name(document->tags,
	    reinterpret_cast<const lxb_char_t*>(tag.data()), tag.size());
}

/// Lightweight attribute/class/id matcher used by find/find_all, replacing a
/// std::function. That target actually fit the small-buffer optimization on our
/// toolchains (a trivially-copyable string_view capture), so the win is not
/// avoided heap allocation but the removed type-erased indirect call: this switch
/// inlines into the DOM walk loop, and we stop relying on an SBO the standard does
/// not guarantee. The match kind is resolved once from the attribute name, then
/// evaluated on each walked node. (Tag matching goes through resolve_tag_id.)
struct AttrValuePredicate {
	enum class Kind { class_contains, class_exact, id, attribute };

	Kind kind;
	std::string_view attr_name; // used only by Kind::attribute

	bool operator()(const DOMElementView& element, std::string_view value) const {
		switch (kind) {
		case Kind::class_contains: return element.contains_class(value);
		case Kind::class_exact:    return element.class_name() == value;
		case Kind::id:             return element.id() == value;
		case Kind::attribute: {
			std::optional<DOMAttrView> attr = element.find_attr(attr_name);
			return attr && attr->value() == value;
		}
		}
		return false;
	}
};

static AttrValuePredicate get_attr_value_predicate(std::string_view name, bool ignore_class_whitespaces) {
	if (name == "class") {
		return { ignore_class_whitespaces ? AttrValuePredicate::Kind::class_contains
		                                  : AttrValuePredicate::Kind::class_exact, {} };
	}
	if (name == "id") {
		return { AttrValuePredicate::Kind::id, {} };
	}
	return { AttrValuePredicate::Kind::attribute, name };
}

/// Collect every descendant element the unary predicate accepts, in document order.
template <typename Match>
static std::vector<DOMElementView> collect_matching(lxb_dom_element_t* element, Match match) {
	std::vector<DOMElementView> out;

	const auto end = DOMElementWalkIterator{};
	for (auto iter = DOMElementWalkIterator(element); iter != end; ++iter) {
		if (match(*iter)) {
			out.push_back(*iter);
		}
	}

	return out;
}

namespace {

/// RAII wrapper over a lexbor selectors search engine.
class SelectorEngine {
public:
	SelectorEngine() {
		engine_ = lxb_selectors_create();
		if (engine_ != nullptr && lxb_selectors_init(engine_) != LXB_STATUS_OK) {
			engine_ = lxb_selectors_destroy(engine_, true);
		}
	}

	SelectorEngine(const SelectorEngine&)            = delete;
	SelectorEngine& operator=(const SelectorEngine&) = delete;

	~SelectorEngine() {
		if (engine_ != nullptr) {
			lxb_selectors_destroy(engine_, true);
		}
	}

	explicit operator bool() const noexcept {
		return engine_ != nullptr;
	}

	lxb_selectors_t* get() const noexcept {
		return engine_;
	}

private:
	lxb_selectors_t* engine_ = nullptr;
};

lxb_status_t query_first_cb(lxb_dom_node_t* node, lxb_css_selector_specificity_t, void* ctx) {
	*static_cast<DOMElementView*>(ctx) = DOMElementView(lxb_dom_interface_element(node));
	return LXB_STATUS_STOP; // first match found, stop the search
}

lxb_status_t query_all_cb(lxb_dom_node_t* node, lxb_css_selector_specificity_t, void* ctx) {
	static_cast<std::vector<DOMElementView>*>(ctx)->emplace_back(lxb_dom_interface_element(node));
	return LXB_STATUS_OK;
}

} // namespace

DOMElementView::DOMElementView(lxb_dom_element_t* element)
    : element_(element) {
	// A null element is a valid (invalid/empty) view, so guard the deref.
	assert(element == nullptr || element->node.type == LXB_DOM_NODE_TYPE_ELEMENT);
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
	assert(element_ && "DOMElementView accessor called on an invalid view");
	size_t size;
	const lxb_char_t* name = lxb_dom_element_tag_name(element_, &size);
	return std::string_view(reinterpret_cast<const char*>(name), size);
}

/// actually might have side effect as some cached allocations
std::string_view DOMElementView::class_name() const {
	assert(element_ && "DOMElementView accessor called on an invalid view");
	size_t size;
	const lxb_char_t* name = lxb_dom_element_class(element_, &size);
	return std::string_view(reinterpret_cast<const char*>(name), size);
}

/// actually might have side effect as some cached allocations
std::string_view DOMElementView::id() const {
	assert(element_ && "DOMElementView accessor called on an invalid view");
	size_t size;
	const lxb_char_t* name = lxb_dom_element_id(element_, &size);
	return std::string_view(reinterpret_cast<const char*>(name), size);
}

std::optional<DOMElementView> DOMElementView::find(std::string_view tag) const {
	lxb_tag_id_t tag_id = resolve_tag_id(element_, tag);
	auto iter = std::find_if(DOMElementWalkIterator(element_), DOMElementWalkIterator{}, [tag_id](const DOMElementView& element) {
		return lxb_dom_element_tag_id(element.get()) == tag_id;
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

std::vector<DOMElementView> DOMElementView::find_all(std::string_view tag) const {
	lxb_tag_id_t tag_id = resolve_tag_id(element_, tag);
	return collect_matching(element_, [tag_id](const DOMElementView& element) {
		return lxb_dom_element_tag_id(element.get()) == tag_id;
	});
}

/// actually might have side effect as some cached allocations
std::vector<DOMElementView> DOMElementView::find_all(std::string_view attr, std::string_view value, bool ignore_class_whitespaces) const {
	AttrValuePredicate predicate = get_attr_value_predicate(attr, ignore_class_whitespaces);

	return collect_matching(element_, [&predicate, value](const DOMElementView& element) {
		return predicate(element, value);
	});
}

std::optional<DOMElementView> DOMElementView::query(const CompiledSelector& selector) const {
	if (element_ == nullptr || !selector) {
		return std::nullopt;
	}
	SelectorEngine engine;
	if (!engine) {
		return std::nullopt;
	}

	DOMElementView found;
	lxb_selectors_find(engine.get(), lxb_dom_interface_node(element_), selector.get(), query_first_cb, &found);
	if (!found) {
		return std::nullopt;
	}
	return found;
}

std::vector<DOMElementView> DOMElementView::query_all(const CompiledSelector& selector) const {
	std::vector<DOMElementView> out;
	if (element_ == nullptr || !selector) {
		return out;
	}
	SelectorEngine engine;
	if (!engine) {
		return out;
	}

	// Match each node once even if it satisfies several selectors of a list.
	lxb_selectors_opt_set(engine.get(), LXB_SELECTORS_OPT_MATCH_FIRST);
	lxb_selectors_find(engine.get(), lxb_dom_interface_node(element_), selector.get(), query_all_cb, &out);
	return out;
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
	auto iter  = std::find_if(std::begin(attrs), std::end(attrs), [&name](const DOMAttrView& attr) {
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
	assert(element_ && "DOMElementView accessor called on an invalid view");
	size_t length;
	const lxb_char_t* text = lxb_dom_node_text_content(lxb_dom_interface_node(element_), &length);
	return std::string_view(reinterpret_cast<const char*>(text), length);
}

/// Keeps a character if it belongs in the collapsed text: drops every whitespace
/// except ' ', and collapses runs of ' ' to a single space. is_last_blank carries
/// the "previous kept char was a space" state, so a single instance must be driven
/// across the whole node walk (like a browser collapsing whitespace across inline
/// element boundaries) rather than restarted per text node.
struct ElementTextGetterPredicate {
	bool operator()(lxb_char_t c) {
		if (std::isspace(c) && c != static_cast<lxb_char_t>(' '))
			return false;
		if (c == static_cast<lxb_char_t>(' ')) {
			return !std::exchange(is_last_blank, true);
		}
		is_last_blank = false;
		return true;
	}

	bool is_last_blank = true;
};

std::string DOMElementView::text() const {
	assert(element_ && "DOMElementView accessor called on an invalid view");
	// Two passes (measure, then fill) share one predicate each so whitespace
	// collapsing persists across text nodes. The passes MUST feed the predicate
	// the exact same character sequence, or the measured and written lengths
	// diverge; keep their node/char handling identical.
	auto measure = [this](ElementTextGetterPredicate& predicate) {
		size_t length = 0;
		for (const DOMNodeView& node : DOMNodeWalkIterator(*this)) {
			if (node.is_element()) {
				length += node.as_element().tag_name() == "BR" ? 1 : 0;
				continue;
			}
			if (!node.is_text()) {
				continue;
			}
			for (lxb_char_t c : node.as_text()) {
				length += predicate(c) ? 1 : 0;
			}
		}
		return length;
	};

	ElementTextGetterPredicate char_predicate;
	std::string output_string(measure(char_predicate), 0);

	auto output_iterator = std::begin(output_string);
	char_predicate       = ElementTextGetterPredicate{};
	for (const DOMNodeView& node : DOMNodeWalkIterator(*this)) {
		if (node.is_element()) {
			if (node.as_element().tag_name() == "BR") {
				*output_iterator++ = '\n';
			}
			continue;
		}
		if (!node.is_text()) {
			continue;
		}
		for (lxb_char_t c : node.as_text()) {
			if (char_predicate(c)) {
				*output_iterator++ = static_cast<char>(c);
			}
		}
	}

	// remove trailing spaces
	size_t last_non_space = output_string.find_last_not_of(' ');
	if (last_non_space != std::string::npos && last_non_space + 1 != output_string.size()) {
		output_string.erase(last_non_space + 1);
	}

	return output_string;
}

DOMElementWalkIterator::DOMElementWalkIterator(lxb_dom_element_t* element)
    : root_(lxb_dom_interface_node(element))
    , node_(root_ ? root_->first_child : nullptr) {
	walk_until_element();
}

DOMElementWalkIterator::DOMElementWalkIterator(const DOMElementView& element)
    : root_(lxb_dom_interface_node(element.element_))
    , node_(root_ ? root_->first_child : nullptr) {
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
	} else {
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

DOMElementIterator::DOMElementIterator(lxb_dom_element_t* element)
    : node_(element ? lxb_dom_interface_node(element)->first_child : nullptr) {
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

bool operator==(const DOMElementView& left, const DOMElementView& right) noexcept {
	return left.element_ == right.element_;
}

bool operator==(const DOMElementIterator& left, const DOMElementIterator& right) {
	return left.node_ == right.node_;
}

DOMElement::DOMElement(lxb_dom_element_t* element)
    : element_(element) {
}

DOMElementFinder::DOMElementFinder(DOMElementView element) noexcept
    : element_(element) {}

DOMElementFinder& DOMElementFinder::find(std::string_view tag) & {
	if (element_) {
		element_ = element_->find(tag);
	}
	return *this;
}

DOMElementFinder&& DOMElementFinder::find(std::string_view tag) && {
	if (element_) {
		element_ = element_->find(tag);
	}
	return std::move(*this);
}

DOMElementFinder& DOMElementFinder::find(
    std::string_view attr,
    std::string_view value,
    bool ignore_class_whitespaces) & {
	if (element_) {
		element_ = element_->find(attr, value, ignore_class_whitespaces);
	}
	return *this;
}

DOMElementFinder&& DOMElementFinder::find(
    std::string_view attr,
    std::string_view value,
    bool ignore_class_whitespaces) && {
	if (element_) {
		element_ = element_->find(attr, value, ignore_class_whitespaces);
	}
	return std::move(*this);
}

DOMElementFinder::operator bool() const noexcept {
	return element_.has_value();
}

DOMElementView& DOMElementFinder::operator*() noexcept {
	return *element_;
}

DOMElementView* DOMElementFinder::operator->() noexcept {
	return element_.operator->();
}

DOMElementView& DOMElementFinder::value() {
	return element_.value();
}
} // namespace aniparse::html