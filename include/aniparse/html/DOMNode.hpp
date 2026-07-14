/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <string_view>

typedef struct lxb_dom_node lxb_dom_node_t;

namespace aniparse::html {

class DOMElementView;
class DOMNodeIterator;
class DOMNodeWalkIterator;

/**
 * @brief Class for accessing DOM node like element and text
 * Supports iterating child nodes. For walking @see DOMNodetWalkIterator
 * @note The class is non owning, so it must be alive with not "View" class
 *
 * The node layer is the one below DOMElementView: it also sees the text nodes
 * between the elements, which is what makes it the right tool when the text of a
 * fragment matters (DOMElementView::text() is built on it).
 *
 * Lifetime: the view borrows into the HTMLDocument the node was parsed from, and
 * so does everything reached through it — the DOMElementView from @ref as_element,
 * the std::string_view from @ref as_text, every neighbour node. The document must
 * outlive them all; to keep text past the document, copy it into a std::string.
 * Once the document is destroyed, every such view and string_view dangles.
 *
 * Validity: a default-constructed view holds no node (@see operator bool). Every
 * accessor below reads the node, so calling any of them on an invalid view is
 * undefined behavior — check the view first. Navigating off the edge of the tree
 * (@ref next of the last sibling, @ref parent of the root) is not an error: it
 * simply returns an invalid view, which then has to be checked before it is used.
 */
class DOMNodeView {
	friend class DOMNodeWalkIterator;
	friend class DOMNodeIterator;
	friend class DOMElementView;
	friend class DOMElementIterator;
	friend class DOMElementIterator;

public:
	/// @note Names DOMNodeIterator, which is only declared, never defined: there is
	///       no child-node iterator yet, and this alias therefore names an
	///       incomplete type. DOMNodeWalkIterator is what actually traverses nodes,
	///       and @ref first_child / @ref next reach the direct children by hand.
	using iterator_type = DOMNodeIterator;

	/**
	 * @brief Make view of DOM node
	 * @param node DOM node
	 */
	DOMNodeView(lxb_dom_node_t* node) noexcept;

	/**
	 * @brief Make view of DOM element node
	 * @param element DOM element
	 */
	DOMNodeView(const DOMElementView& element) noexcept;

	/**
	 * @brief Make empty (invalid) DOM node view
	 * @note You can't use getters with invalid view.
	 */
	DOMNodeView() noexcept = default;

	/**
	 * @brief Copy view of the same DOM node
	 * @param other DOM node
	 */
	DOMNodeView(const DOMNodeView& other) noexcept = default;

	/**
	 * @brief Copy view of the same DOM node
	 * @param other DOM node
	 */
	DOMNodeView(DOMNodeView&& other) noexcept = default;

	/**
	 * @brief destruct view, no free or clean
	 */
	~DOMNodeView() noexcept = default;

	/**
	 * @brief Copy view of the same DOM node
	 * @param other DOM node
	 * @return *this
	 */
	DOMNodeView& operator=(const DOMNodeView& other) & noexcept = default;

	/**
	 * @brief Copy view of the same DOM node
	 * @param other DOM node
	 * @return *this
	 */
	DOMNodeView& operator=(DOMNodeView&& other) & noexcept = default;

	/**
	 * @return true if DOM node is valid, false otherwise
	 */
	[[nodiscard]] operator bool() const noexcept;

	/**
	 * @brief Get raw pointer to the DOM node
	 *        implementation defined struct.
	 * @note Use it only if you *have to*. This
	 *       pointer should be implementation defined
	 * @return Raw pointer to the DOM element
	 */
	[[nodiscard]] lxb_dom_node_t* get() const noexcept;

	/**
	 * @brief Whether the node is an element (a tag), and so convertible with
	 *        @ref as_element.
	 * @pre The view must be valid; otherwise UB.
	 * @return true if the node is an element node, false for text, comments, and
	 *         every other node kind
	 */
	[[nodiscard]] bool is_element() const noexcept;

	/**
	 * @brief Whether the node is a text node, and so readable with @ref as_text.
	 * @pre The view must be valid; otherwise UB.
	 * @return true if the node is a text node, false otherwise
	 */
	[[nodiscard]] bool is_text() const noexcept;

	/**
	 * @brief The node seen as an element.
	 * @pre The view must be valid; otherwise UB. Check @ref is_element first
	 *      unless the node kind is already known.
	 * @throw std::runtime_error If the node is not an element node
	 * @note The returned view borrows into the same HTMLDocument; it dangles once
	 *       that document dies.
	 * @return DOM element view of this node
	 */
	DOMElementView as_element() const;

	/**
	 * @brief The character data of a text node, exactly as parsed: no whitespace
	 *        collapsing, no trimming, no entity of any kind re-encoded.
	 * @pre The view must be valid; otherwise UB. Check @ref is_text first unless
	 *      the node kind is already known.
	 * @throw std::runtime_error If the node is not a text node
	 * @note The result borrows into the owning HTMLDocument and dangles once that
	 *       document dies; copy it into a std::string to keep it.
	 * @return Text of the node
	 */
	std::string_view as_text() const;

	/**
	 * @brief The node's first child, of any kind (an element, a text node, ...).
	 * @pre The view must be valid; otherwise UB.
	 * @return View of the first child, or an invalid view if the node has none
	 */
	DOMNodeView first_child() const noexcept;

	/**
	 * @brief The node that contains this one.
	 * @pre The view must be valid; otherwise UB.
	 * @return View of the parent, or an invalid view at the top of the tree
	 */
	DOMNodeView parent() const noexcept;

	/**
	 * @brief The previous sibling, of any kind.
	 * @pre The view must be valid; otherwise UB.
	 * @return View of the previous sibling, or an invalid view if this node is the
	 *         first child of its parent
	 */
	DOMNodeView prev() const noexcept;

	/**
	 * @brief The next sibling, of any kind.
	 * @pre The view must be valid; otherwise UB.
	 * @return View of the next sibling, or an invalid view if this node is the last
	 *         child of its parent
	 */
	DOMNodeView next() const noexcept;

	/**
	 * @brief Compare two node views
	 * @param left  Left node view
	 * @param right Right node view
	 * @return true if both view the same node (or are both invalid), false
	 *         otherwise. This is node identity, not structural equality: two
	 *         distinct nodes with identical markup do not compare equal
	 */
	friend bool operator==(const DOMNodeView& left, const DOMNodeView& right) noexcept;

private:
	lxb_dom_node_t* node_ = nullptr;
};

/**
 * @brief Class for *recursively* iterating (walking)
 *        through all the DOM node children.
 * @note Becomes invalid when reaches end
 *       or node is destroyed
 *
 * Yields every descendant node — elements and text alike — in document order,
 * depth first. The node the iterator was built from is not yielded, only what it
 * contains. Together with the free begin()/end() overloads it is usable directly
 * in a range-for. It borrows into the HTMLDocument, which must outlive it.
 */
class DOMNodeWalkIterator {
public:
	/// What the walk yields: a view of one node of the subtree.
	using value_type        = DOMNodeView;
	/// Signed difference type, as the iterator concepts require.
	using difference_type   = std::ptrdiff_t;
	/// Pointer type the dereference operator yields.
	using pointer           = const DOMNodeView*;
	/// Reference type the dereference operator yields.
	using reference         = const DOMNodeView&;
	/// The walk only moves forward: a tree is descended, never rewound.
	using iterator_category = std::forward_iterator_tag;

	/**
	 * @brief Initialize iterator from the
	 *        first child of given node
	 * @param node DOM node raw pointer
	 */
	explicit DOMNodeWalkIterator(lxb_dom_node_t* node) noexcept;

	/**
	 * @brief Initialize iterator from the
	 *        first child of given node
	 * @param node DOM node
	 */
	explicit DOMNodeWalkIterator(DOMNodeView node) noexcept;

	/**
	 * @brief Construct invalid interator.
	 * Represents end of a normal iterator
	 */
	DOMNodeWalkIterator() noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 */
	DOMNodeWalkIterator(const DOMNodeWalkIterator& other) noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 */
	DOMNodeWalkIterator(DOMNodeWalkIterator&& other) noexcept = default;

	/**
	 * @brief Destruct iterator
	 */
	~DOMNodeWalkIterator() noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 * @return *this
	 */
	DOMNodeWalkIterator& operator=(const DOMNodeWalkIterator& other) & noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 * @return *this
	 */
	DOMNodeWalkIterator& operator=(DOMNodeWalkIterator&& other) & noexcept = default;

	/**
	 * @return DOM element view of the current element
	 */
	[[nodiscard]] const DOMNodeView& operator*() const noexcept;

	/**
	 * @return DOM element view pointer of the current element
	 */
	[[nodiscard]] const DOMNodeView* operator->() const noexcept;

	/**
	 * @brief Walk to next element
	 * @return *this
	 */
	DOMNodeWalkIterator& operator++() noexcept;

	/**
	 * @brief Walk to next element
	 * @return Copy of this iterator with next element
	 */
	DOMNodeWalkIterator operator++(int) noexcept;

	/**
	 * @brief Compare iterator
	 * @return true if iterators are at the same element
	 *         or both invalid, false otherwise
	 */
	friend bool operator==(const DOMNodeWalkIterator& left, const DOMNodeWalkIterator& right) noexcept;

private:
	/**
	 * @brief Walk to next node
	 */
	void next();

	DOMNodeView root_ = nullptr;
	DOMNodeView node_ = nullptr;
};

/**
 * @brief Make a walk iterator usable as a range, so it can be fed to a range-for
 *        directly instead of spelling out a begin/end pair.
 * @param iter The iterator, already positioned at the start of the walk
 * @return @p iter unchanged
 */
[[nodiscard]] inline DOMNodeWalkIterator begin(DOMNodeWalkIterator iter) {
	return iter;
}

/**
 * @brief End of the range formed by a walk iterator.
 * @return A default-constructed (invalid) iterator, which the walk compares equal
 *         to once the whole subtree has been visited
 */
[[nodiscard]] inline DOMNodeWalkIterator end(const DOMNodeWalkIterator&) {
	return {};
}
} // namespace aniparse::html