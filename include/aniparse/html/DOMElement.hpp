/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <string>
#include <string_view>
#include <optional>
#include <vector>

typedef struct lxb_dom_element lxb_dom_element_t;
typedef struct lxb_dom_node lxb_dom_node_t;

namespace aniparse::html {
class DOMAttrView;
class DOMElementAttrsView;
class CompiledSelector;

class DOMElementIterator;

/**
 * @brief Class for accessing DOM element attributes, name, etc.
 * Supports iterating child elements. For walking @see DOMElementWalkIterator
 * @note The class is non owning, so it must be alive with not "View" class
 *
 * Validity contract (a default-constructed view is invalid, @see operator bool):
 * - Scalar accessors that read this element's own data (tag_name, class_name,
 *   id, contains_class, content_text, text) have a narrow contract: the view
 *   MUST be valid. Calling them on an invalid view is undefined behavior
 *   (asserted in debug builds). Check operator bool() first.
 * - Search and iteration methods (find, find_all, query, query_all, get_attr,
 *   find_attr, attributes, begin/end) are total: on an invalid view they simply
 *   return an empty result (nullopt / empty container / an empty range). This is
 *   what lets a chain of steps be checked once, at its end, rather than after
 *   every step.
 *
 * Lifetime (the contract to get right, because breaking it is a use-after-free):
 * a DOMElementView owns nothing. It points into the HTMLDocument that parsed the
 * markup, and so does everything reached through it — the views returned by find,
 * query, attributes and the iterators, and every std::string_view handed back by
 * tag_name, class_name, id, content_text, DOMAttrView::value. Those bytes live in
 * the document's arena. The document must therefore outlive every view and every
 * string_view taken from it: keep it alive for the whole parse, and copy anything
 * that has to survive it into a std::string (which is what text() already returns,
 * being the one accessor that hands back owned data) before letting it die.
 * Storing a view or a string_view in the parsed model and dropping the document is
 * the mistake this note exists to prevent.
 */
class DOMElementView {
	friend class DOMNodeView;
	friend class DOMElementWalkIterator;
	friend class DOMElementIterator;

public:
	/// Iterator over this element's direct child elements (@see begin, end).
	using iterator_type = DOMElementIterator;

	/**
	 * @brief Make view of DOM element
	 * @param element DOM element
	 */
	DOMElementView(lxb_dom_element_t* element);

	/**
	 * @brief Make empty (invalid) DOM element view
	 * @note Scalar accessors (tag_name, class_name, id, contains_class,
	 *       content_text, text) require a valid view; the search/iteration
	 *       methods are total. @see the class-level validity contract.
	 */
	DOMElementView() = default;

	/**
	 * @brief Copy view of the same DOM element
	 * @param other DOM element
	 */
	DOMElementView(const DOMElementView& other) = default;

	/**
	 * @brief Move view of the same DOM element
	 * @param other DOM element
	 */
	DOMElementView(DOMElementView&& other) = default;

	/**
	 * @brief destruct view, no free or clean
	 */
	~DOMElementView() noexcept = default;

	/**
	 * @brief Copy view of the same DOM element
	 * @param other DOM element
	 * @return *this
	 */
	DOMElementView& operator=(const DOMElementView& other) & noexcept = default;

	/**
	 * @brief Move view of the same DOM element
	 * @param other DOM element
	 * @return *this
	 */
	DOMElementView& operator=(DOMElementView&& other) & noexcept = default;

	/**
	 * @return true if DOM element is valid, false otherwise
	 */
	[[nodiscard]] operator bool() const;

	/**
	 * @brief Range over the element's attributes, in the order the parser stored
	 *        them.
	 * @note The range and the DOMAttrView it yields borrow into the owning
	 *       HTMLDocument, which must outlive them.
	 * @pre Iterating the result requires a valid view: on an invalid one, the
	 *      returned range must not be iterated (it would dereference no element).
	 * @return View of the DOM element attributes
	 */
	[[nodiscard]] DOMElementAttrsView attributes() const;

	/**
	 * @brief Iterator to the start (first) of DOM element children
	 * Direct child *elements* only — text nodes between them are skipped; use
	 * DOMNodeWalkIterator when the text matters, and DOMElementWalkIterator to
	 * descend below the direct children.
	 * @note The iterator and the views it yields borrow into the owning
	 *       HTMLDocument, which must outlive them.
	 * @return DOM elements iterator; equal to @ref end when the element has no
	 *         child elements, or when the view is invalid
	 */
	[[nodiscard]] DOMElementIterator begin();

	/**
	 * @brief Iterator to the end of DOM element children.
	 * @note Taking value from it will be UB
	 * @return DOM elements iterator
	 */
	[[nodiscard]] DOMElementIterator end();

	/**
	 * @pre The view must be valid (@see operator bool); otherwise UB.
	 * @note Tag name always in upper case ("HTML", "DIV", etc.)
	 * @note The result borrows into the owning HTMLDocument and dangles once that
	 *       document dies; copy it into a std::string to keep it.
	 * @return DOM element tag/name, e.g. \<html\> => HTML
	 */
	[[nodiscard]] std::string_view tag_name() const;

	/**
	 * @brief Get the full class name of the element.
	 * This is just efficient shortcut to "class" attr, get_attr("class")
	 * The classes may be separated by whitespace characters
	 * @pre The view must be valid (@see operator bool); otherwise UB.
	 * @note The result borrows into the owning HTMLDocument and dangles once that
	 *       document dies; copy it into a std::string to keep it.
	 * @return DOM element class, empty if the element carries no class attribute
	 */
	[[nodiscard]] std::string_view class_name() const;

	/**
	 * @brief Get the id name of the element.
	 * This is just efficient shortcut to "id" attr, get_attr("id")
	 * @pre The view must be valid (@see operator bool); otherwise UB.
	 * @note The result borrows into the owning HTMLDocument and dangles once that
	 *       document dies; copy it into a std::string to keep it.
	 * @return DOM element id, empty if the element carries no id attribute
	 */
	[[nodiscard]] std::string_view id() const;

	/**
	 * @brief Check if the element contains class.
	 * Checks with respect to whitespaces.
	 * @pre The view must be valid (@see operator bool); otherwise UB.
	 * @param name Class name to check
	 * @return true if the class is found, false otherwise
	 */
	[[nodiscard]] bool contains_class(std::string_view name) const;

	/**
	 * @brief Find first element with tag
	 * Searches the descendants depth first, in document order; the element itself
	 * is not matched.
	 * @note Tag matching is case insensitive, so "div", "DIV" and "Div" are equivalent.
	 * @note The returned view borrows into the owning HTMLDocument, which must
	 *       outlive it.
	 * @param tag DOM element tag name
	 * @return DOM element view if found, nullopt otherwise (including on an invalid
	 *         view and for a tag no element carries)
	 */
	[[nodiscard]] std::optional<DOMElementView> find(std::string_view tag) const;

	/**
	 * @brief Find first element with attribute and value
	 * Searches the descendants depth first, in document order; the element itself
	 * is not matched. The value must match in full — this is equality, not a
	 * substring test (the sole exception being the class rule below).
	 * @note The returned view borrows into the owning HTMLDocument, which must
	 *       outlive it.
	 * @param attr DOM attribute name
	 * @param value DOM Attrubute value
	 * @param ignore_class_whitespaces Only for "attr" == "class".
	 *        If true, then it checks if element contains "value" class,
	 *        Otherwise, checks if element class exactly the same
	 * @return DOM element view if found, nullopt otherwise (including on an invalid
	 *         view)
	 */
	[[nodiscard]] std::optional<DOMElementView> find(
	    std::string_view attr,
	    std::string_view value,
	    bool ignore_class_whitespaces = true) const;

	/**
	 * @brief Find the first descendant matching a CSS selector.
	 * @note The element itself is not matched, only its descendants.
	 * @note The returned view borrows into the owning HTMLDocument, which must
	 *       outlive it.
	 * @param selector Compiled CSS selector
	 * @return DOM element view if found, nullopt otherwise — also when the view is
	 *         invalid, when the selector failed to compile, or when the search
	 *         engine could not be created
	 */
	[[nodiscard]] std::optional<DOMElementView> query(const CompiledSelector& selector) const;

	/**
	 * @brief Find all descendants matching a CSS selector, in document order.
	 * @note The element itself is not matched, only its descendants.
	 *       Each matching element appears once even if it matches several
	 *       selectors of a comma separated list.
	 * @note The returned views borrow into the owning HTMLDocument, which must
	 *       outlive them and the vector alike.
	 * @param selector Compiled CSS selector
	 * @return All found DOM element views; empty when nothing matches, when the
	 *         view is invalid, or when the selector failed to compile
	 */
	[[nodiscard]] std::vector<DOMElementView> query_all(const CompiledSelector& selector) const;

	/**
	 * @brief Find all elements with tag
	 * Searches the descendants depth first, in document order; the element itself
	 * is not matched.
	 * @note Tag matching is case insensitive, so "div", "DIV" and "Div" are equivalent.
	 * @note The returned views borrow into the owning HTMLDocument, which must
	 *       outlive them.
	 * @param tag DOM element tag name
	 * @return All found DOM element views; empty when nothing matches or the view
	 *         is invalid
	 */
	[[nodiscard]] std::vector<DOMElementView> find_all(std::string_view tag) const;

	/**
	 * @brief Find all elements with attribute and value
	 * Searches the descendants depth first, in document order; the element itself
	 * is not matched.
	 * @note The returned views borrow into the owning HTMLDocument, which must
	 *       outlive them.
	 * @param attr DOM attribute name
	 * @param value DOM Attrubute value
	 * @param ignore_class_whitespaces Only for "attr" == "class".
	 *        If true, then it checks if element contains "value" class,
	 *        Otherwise, checks if element class exactly the same
	 * @return All found DOM element views; empty when nothing matches or the view
	 *         is invalid
	 */
	[[nodiscard]] std::vector<DOMElementView> find_all(
	    std::string_view attr,
	    std::string_view value,
	    bool ignore_class_whitespaces = true) const;

	/**
	 * @brief Find DOM element attribute with name.
	 * Looks at this element's own attributes only, never at its descendants; the
	 * name must match exactly (HTML parsing lowercases attribute names).
	 * @note to access "class" or "id" it is better to use @ref class_name() and @ref id()
	 * @note The returned view borrows into the owning HTMLDocument, which must
	 *       outlive it.
	 * @param name DOM attribute name
	 * @return DOM attribute view if found, nullopt otherwise (an invalid view has
	 *         no attributes, so it yields nullopt rather than misbehaving)
	 */
	[[nodiscard]] std::optional<DOMAttrView> find_attr(std::string_view name) const;

	/**
	 * @brief Get DOM element attribute value with name.
	 * Distinguishes an absent attribute (nullopt) from a present but valueless one
	 * (an engaged optional holding an empty string_view), which is what makes it
	 * usable for boolean attributes.
	 * @note to access "class" or "id" it is better to use @ref class_name() and @ref id()
	 * @note The returned string_view borrows into the owning HTMLDocument and
	 *       dangles once that document dies; copy it into a std::string to keep it.
	 * @param name DOM attribute name
	 * @return Value of the element or empty string
	 *         if attribute found, std::nullopt otherwise
	 */
	[[nodiscard]] std::optional<std::string_view> get_attr(std::string_view name) const;

	/**
	 * @brief Get text of element.
	 * Walks though all chilren of the element.
	 * Ignores all element tags and print all it's contents.
	 * Also preserves all whitespace characters
	 * @pre The view must be valid (@see operator bool); otherwise UB.
	 * @note The result borrows into the owning HTMLDocument and dangles once that
	 *       document dies; copy it into a std::string to keep it. Reading it also
	 *       makes the document cache the concatenated text, so the document is
	 *       mutated even though the view is const.
	 * @return Text of the element or empty string
	 */
	[[nodiscard]] std::string_view content_text() const;

	/**
	 * @brief Get escaped text of element.
	 * Walks though all chilren of the element.
	 * Ignores all new line characters,
	 * removes duplicated space (blank) characters
	 * and removes first leading and
	 * last trailing spaces.
	 * Interprets <BR> element as new line ('\n').
	 * @pre The view must be valid (@see operator bool); otherwise UB.
	 * @note Unlike every other accessor here, the result is owned: a std::string
	 *       that outlives the document. This is the accessor to reach for when the
	 *       text goes into a parsed model rather than being consumed on the spot.
	 * @return Text of the element or empty string
	 */
	[[nodiscard]] std::string text() const;

	//std::string_view to_string() const;

	/**
	 * @brief Get raw pointer to the DOM element
	 *        implementation defined struct.
	 * @note Use it only if you *have to*. This
	 *       pointer should be implementation defined
	 * @note Non-owning, like the view itself: the pointer is only valid while the
	 *       owning HTMLDocument lives.
	 * @return Raw pointer to the DOM element, nullptr for an invalid view
	 */
	[[nodiscard]] lxb_dom_element_t* get() const;

	/**
	 * @brief compare two elements
	 * @return true if elements are the same, false otherwise
	 */
	friend bool operator==(const DOMElementView& left, const DOMElementView& right) noexcept;

private:
	lxb_dom_element_t* element_ = nullptr;
};

/**
 * @brief Class for iterating through all element children.
 * Supports bidirectional iterating
 * @note Becomes invalid when reaches start or end
 *
 * Direct child *elements* only: the text nodes between them are skipped, and the
 * children of those children are not descended into (DOMElementWalkIterator does
 * that). It borrows into the HTMLDocument, which must outlive both the iterator
 * and the views it yields.
 */
class DOMElementIterator {
public:
	/// What the iteration yields: a view of one child element.
	using value_type        = DOMElementView;
	/// Signed difference type, as the iterator concepts require.
	using difference_type   = std::ptrdiff_t;
	/// Pointer handed back by operator->.
	using pointer           = const DOMElementView*;
	/// Reference handed back by operator*.
	using reference         = const DOMElementView&;
	/// Children form a doubly-linked list, so the walk goes both ways.
	using iterator_category = std::bidirectional_iterator_tag;

	/**
	 * @see DOMElementView::begin
	 * @brief Initialize iterator from the
	 *        first child of given element
	 * @param element DOM element raw pointer
	 */
	explicit DOMElementIterator(lxb_dom_element_t* element);

	/**
	 * @brief Construct invalid interator.
	 * Represents end of a normal iterator
	 */
	DOMElementIterator() noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 */
	DOMElementIterator(const DOMElementIterator& other) noexcept = default;

	/**
	 * @brief Move state of other iterator
	 * @param other Iterator to move
	 */
	DOMElementIterator(DOMElementIterator&& other) noexcept = default;

	/**
	 * @brief Destruct iterator
	 */
	~DOMElementIterator() noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 * @return *this
	 */
	DOMElementIterator& operator=(const DOMElementIterator& other) & = default;

	/**
	 * @brief Move state of other iterator
	 * @param other Iterator to move
	 * @return *this
	 */
	DOMElementIterator& operator=(DOMElementIterator&& other) & noexcept = default;

	/**
	 * @note The reference is into the iterator itself and is invalidated by the
	 *       next increment; copy the DOMElementView out to keep it (the view in
	 *       turn borrows into the HTMLDocument).
	 * @return DOM element view of the current element
	 */
	[[nodiscard]] const DOMElementView& operator*() const;

	/**
	 * @return DOM element view of the current element
	 */
	[[nodiscard]] const DOMElementView* operator->() const;

	/**
	 * @brief Iterate back to previous DOM element
	 * @return *this
	 */
	DOMElementIterator& operator--();

	/**
	 * @brief Iterate back to previous DOM element
	 * @return Copy of this iterator with next element
	 */
	DOMElementIterator operator--(int);

	/**
	 * @brief Iterate to next child element
	 * @return *this
	 */
	DOMElementIterator& operator++();

	/**
	 * @brief Iterate to next child element
	 * @return Copy of this iterator with next element
	 */
	DOMElementIterator operator++(int);

	/**
	 * @brief Compare iterator
	 * @return true if iterators are at the same element
	 *         or both invalid, false otherwise
	 */
	friend bool operator==(const DOMElementIterator& left, const DOMElementIterator& right);

private:
	/**
	 * @brief Iterate to DOM element if current
	 *        node isn't DOM element
	 */
	void iterate_until_element();

	lxb_dom_node_t* node_ = nullptr;
	DOMElementView node_view_;
};

/**
 * @brief Class for *recursively* iterating (walking)
 *        through all the DOM element children.
 * @note Becomes invalid when reaches end
 *       or element is destroyed
 *
 * Yields every descendant *element* in document order, depth first, skipping text
 * nodes; the element it was built from is not yielded, only what it contains. This
 * is what find/find_all are built on. Together with the free begin()/end()
 * overloads it works directly in a range-for. It borrows into the HTMLDocument,
 * which must outlive both the iterator and the views it yields.
 */
class DOMElementWalkIterator {
public:
	/// What the walk yields: a view of one descendant element.
	using value_type        = DOMElementView;
	/// Signed difference type, as the iterator concepts require.
	using difference_type   = std::ptrdiff_t;
	/// Pointer type the dereference operator yields.
	using pointer           = const DOMElementView*;
	/// Reference type the dereference operator yields.
	using reference         = const DOMElementView&;
	/// The walk only moves forward: a tree is descended, never rewound.
	using iterator_category = std::forward_iterator_tag;

	/**
	 * @brief Initialize iterator from the
	 *        first child of given element
	 * @param element DOM element raw pointer
	 */
	explicit DOMElementWalkIterator(lxb_dom_element_t* element);

	/**
	 * @brief Initialize iterator from the
	 *        first child of given element
	 * @param element DOM element
	 */
	explicit DOMElementWalkIterator(const DOMElementView& element);

	/**
	 * @brief Construct invalid interator.
	 * Represents end of a normal iterator
	 */
	DOMElementWalkIterator() noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 */
	DOMElementWalkIterator(const DOMElementWalkIterator& other) noexcept = default;

	/**
	 * @brief Move state of other iterator
	 * @param other Iterator to move
	 */
	DOMElementWalkIterator(DOMElementWalkIterator&& other) noexcept = default;

	/**
	 * @brief Destruct iterator
	 */
	~DOMElementWalkIterator() noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 * @return *this
	 */
	DOMElementWalkIterator& operator=(const DOMElementWalkIterator& other) & = default;

	/**
	 * @brief Move state of other iterator
	 * @param other Iterator to move
	 * @return *this
	 */
	DOMElementWalkIterator& operator=(DOMElementWalkIterator&& other) & noexcept = default;

	/**
	 * @note The reference is into the iterator itself and is invalidated by the
	 *       next increment; copy the DOMElementView out to keep it (the view in
	 *       turn borrows into the HTMLDocument).
	 * @return DOM element view of the current element
	 */
	[[nodiscard]] const DOMElementView& operator*() const;

	/**
	 * @return DOM element view pointer of the current element
	 */
	[[nodiscard]] const DOMElementView* operator->() const;

	/**
	 * @brief Walk to next element
	 * @return *this
	 */
	DOMElementWalkIterator& operator++();

	/**
	 * @brief Walk to next element
	 * @return Copy of this iterator with next element
	 */
	DOMElementWalkIterator operator++(int);

	/**
	 * @brief Compare iterator
	 * @return true if iterators are at the same element
	 *         or both invalid, false otherwise
	 */
	friend bool operator==(const DOMElementWalkIterator& left, const DOMElementWalkIterator& right) noexcept;

private:
	/**
	 * @brief Walk to next element without 
	 */
	void next();

	/**
	 * @brief Walk to DOM element if current
	 *        node isn't DOM element
	 */
	void walk_until_element();

	lxb_dom_node_t* root_ = nullptr;
	lxb_dom_node_t* node_ = nullptr;
	DOMElementView node_view_;
};

/**
 * @brief Make a walk iterator usable as a range, so it can be fed to a range-for
 *        directly instead of spelling out a begin/end pair.
 * @param iter The iterator, already positioned at the start of the walk
 * @return @p iter unchanged
 */
[[nodiscard]] inline DOMElementWalkIterator begin(DOMElementWalkIterator iter) noexcept {
	return iter;
}

/**
 * @brief End of the range formed by a walk iterator.
 * @return A default-constructed (invalid) iterator, which the walk compares equal
 *         to once the whole subtree has been visited
 */
[[nodiscard]] inline DOMElementWalkIterator end(const DOMElementWalkIterator&) noexcept {
	return {};
}

/**
 * @brief Helper class to find DOM elements in chain
 * Used when you want to find an element in another elements
 *
 * Each find() step searches inside whatever the previous step landed on, so a
 * descent reads as one expression instead of a staircase of optionals. A step
 * that finds nothing empties the finder, and every later step is then a no-op:
 * the whole chain fails as a unit and is checked once, at the end, with
 * operator bool() or value().
 *
 * It holds a DOMElementView, so it borrows into the HTMLDocument like everything
 * else here — the document must outlive the finder and whatever is pulled out of
 * it.
 */
class DOMElementFinder {
public:
	/**
	 * @brief Construct finder with element
	 * @param element First element to begin search
	 */
	DOMElementFinder(DOMElementView element) noexcept;

	/**
	 * @brief Find element with tag
	 * @see DOMElementView::find
	 * @param tag DOM element tag name
	 * @return *this, holding the found element, or emptied if nothing matched (or
	 *         if the chain had already failed)
	 */
	DOMElementFinder& find(std::string_view tag) &;

	/**
	 * @brief Find element with tag
	 * @see DOMElementView::find
	 * @param tag DOM element tag name
	 * @return This finder, for chaining another step onto a temporary
	 */
	DOMElementFinder&& find(std::string_view tag) &&;

	/**
	 * @brief Find element with attribute and value
	 * @see DOMElementView::find
	 * @param attr DOM attribute name
	 * @param value DOM attribute value
	 * @param ignore_class_whitespaces Only for "attr" == "class". If true, checks
	 *        whether the element contains the "value" class; otherwise the class
	 *        must match exactly
	 * @return *this, holding the found element, or emptied if nothing matched (or
	 *         if the chain had already failed)
	 */
	DOMElementFinder& find(
	    std::string_view attr,
	    std::string_view value,
	    bool ignore_class_whitespaces = true) &;

	/**
	 * @brief Find element with attribute and value
	 * @see DOMElementView::find
	 * @param attr DOM attribute name
	 * @param value DOM attribute value
	 * @param ignore_class_whitespaces Only for "attr" == "class". If true, checks
	 *        whether the element contains the "value" class; otherwise the class
	 *        must match exactly
	 * @return This finder, for chaining another step onto a temporary
	 */
	DOMElementFinder&& find(
	    std::string_view attr,
	    std::string_view value,
	    bool ignore_class_whitespaces = true) &&;

	/**
	 * @return true if the chain is still standing on an element, false if any step
	 *         of it found nothing
	 */
	operator bool() const noexcept;

	/**
	 * @brief Dereference value in finder.
	 * @note You must always check if current element is valid through "operator bool()".
	 *       Or you can use value() instead
	 * @return Current element
	 */
	DOMElementView& operator*() noexcept;

	/**
	 * @brief Dereference value in finder.
	 * @note You must always check if current element is valid through "operator bool()".
	 *       Or you can use value() instead
	 * @return Current element
	 */
	DOMElementView* operator->() noexcept;

	/**
	 * @brief Dereference value in finder or throw an exception
	 * @throw std::bad_optional_access If current element invalid
	 * @return Current element
	 */
	DOMElementView& value();

private:
	std::optional<DOMElementView> element_;
};
} // namespace aniparse::html