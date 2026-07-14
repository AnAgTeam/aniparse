/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <string_view>
#include <optional>

typedef struct lxb_dom_attr lxb_dom_attr_t;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace aniparse::html {
class DOMElementView;
class DOMAttrsIterator;

/**
 * @brief Class for accessing the DOM element attribute
 * @note The class is non owning, so it must be alive with not "View" class
 *
 * Lifetime: the view points into the HTMLDocument the attribute was parsed from,
 * and so does every std::string_view it hands back (@ref name, @ref value) — the
 * bytes live in the document's arena, not in the view. The document must outlive
 * the view and every string_view taken from it; to keep an attribute past the
 * document, copy it into a std::string first. Destroying the document turns every
 * such view and string_view into a dangling reference.
 */
class DOMAttrView {
	friend class DOMAttrsIterator;
	friend bool operator==(const DOMAttrsIterator& left, const DOMAttrsIterator& right);

public:
	/**
	 * @brief Make view of DOM element attribute
	 * @param attr DOM element attribute
	 */
	DOMAttrView(lxb_dom_attr_t* attr);

	/**
	 * @brief Make empty (invalid) DOM
	 *        element attribute view
	 * @note You can't use getters with invalid view.
	 */
	DOMAttrView() = default;

	/**
	 * @brief Copy view of the same attribute
	 * @param other DOM element attribute
	 */
	DOMAttrView(const DOMAttrView& other) = default;

	/**
	 * @brief Copy view of the same attribute
	 * @param other DOM element attribute
	 */
	DOMAttrView(DOMAttrView&& other) = default;

	/**
	 * @brief destruct view, no free or clean
	 */
	~DOMAttrView() noexcept = default;

	/**
	 * @brief Copy view of the same attribute
	 * @param other DOM element attribute
	 * @return *this
	 */
	DOMAttrView& operator=(const DOMAttrView& other) & = default;

	/**
	 * @brief Copy view of the same attribute
	 * @param other DOM element attribute
	 * @return *this
	 */
	DOMAttrView& operator=(DOMAttrView&& other) & = default;

	/**
	 * @brief Local name of the attribute, as the parser stored it (HTML parsing
	 *        lowercases attribute names, so "HREF" in the markup reads back "href").
	 * @pre The view must be valid; a default-constructed view holds no attribute
	 *      and calling this on it is undefined behavior.
	 * @note The result borrows into the owning HTMLDocument and dangles once that
	 *       document dies; copy it to keep it.
	 * @return Name of the attribute
	 */
	[[nodiscard]] std::string_view name() const;

	/**
	 * @brief Value of the attribute.
	 * @pre The view must be valid; a default-constructed view holds no attribute
	 *      and calling this on it is undefined behavior.
	 * @note The result borrows into the owning HTMLDocument and dangles once that
	 *       document dies; copy it to keep it.
	 * @return Value of the attribute, empty for a valueless attribute (e.g. the
	 *         bare "disabled" of \<input disabled\>)
	 */
	[[nodiscard]] std::string_view value() const;

private:
	lxb_dom_attr_t* attr_ = nullptr;
};

/**
 * @brief Class for accessing all the DOM element attributes
 * @note The class is non owning, so it must be alive with not "View" class
 *
 * A range over one element's attributes, in the order the parser stored them.
 * Obtained from DOMElementView::attributes(); it borrows into the same
 * HTMLDocument as the element, and so does every DOMAttrView it yields.
 */
class DOMElementAttrsView {
	friend class DOMElementView;
	friend class DOMElement;

public:
	/**
	 * @brief Make a view of one element's attributes
	 * @param element The element whose attributes are iterated; nullptr makes an
	 *                empty (invalid) view, which must not be iterated
	 */
	DOMElementAttrsView(lxb_dom_element_t* element);

	/**
	 * @brief Make an empty (invalid) attributes view
	 * @note It refers to no element, so it must not be iterated (@see begin).
	 */
	DOMElementAttrsView() = default;

	/**
	 * @brief Copy view of the same element's attributes
	 * @param other Attributes view to copy
	 */
	DOMElementAttrsView(const DOMElementAttrsView& other) = default;

	/**
	 * @brief Move view of the same element's attributes
	 * @param other Attributes view to move
	 */
	DOMElementAttrsView(DOMElementAttrsView&& other) = default;

	/**
	 * @brief destruct view, no free or clean
	 */
	~DOMElementAttrsView() = default;

	/**
	 * @brief Iterator to the element's first attribute.
	 * @pre The view must refer to an element: begin() dereferences it, so calling
	 *      it on a default-constructed view (or one built from a nullptr element,
	 *      as DOMElementView::attributes() returns for an invalid element view) is
	 *      undefined behavior.
	 * @return Attributes iterator; equal to @ref end when the element carries no
	 *         attributes
	 */
	[[nodiscard]] DOMAttrsIterator begin();

	/**
	 * @brief Iterator past the element's last attribute.
	 * @note It holds no attribute, so dereferencing or advancing it is UB.
	 * @return Attributes end iterator
	 */
	[[nodiscard]] DOMAttrsIterator end();

private:
	lxb_dom_element_t* element_ = nullptr;
};

/**
 * @brief Class for iterating through DOM element attributes
 * @note Becomes invalid when reaches start or end
 *
 * Walks one element's attribute list; the DOMAttrView it yields borrows into the
 * owning HTMLDocument, which must outlive both the iterator and the view.
 */
class DOMAttrsIterator {
public:
	/// @note Names DOMElementView, while dereferencing the iterator actually yields
	///       a DOMAttrView (see @ref reference and @ref pointer, which agree with
	///       the dereference operators). Prefer those; do not build on value_type.
	using value_type        = DOMElementView;
	/// Signed difference type, as the iterator concepts require.
	using difference_type   = std::ptrdiff_t;
	/// Pointer type the dereference operator yields: a view of one attribute.
	using pointer           = const DOMAttrView*;
	/// Reference type the dereference operator yields: a view of one attribute.
	using reference         = const DOMAttrView&;
	/// Attributes form a doubly-linked list, so the walk goes both ways.
	using iterator_category = std::bidirectional_iterator_tag;

	/**
	 * @brief Make an iterator positioned at an attribute
	 * @param attr The attribute to point at; nullptr makes the end iterator
	 */
	DOMAttrsIterator(lxb_dom_attr_t* attr);

	/**
	 * @brief Construct invalid iterator.
	 * Represents the end of a normal iterator
	 */
	DOMAttrsIterator() noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 */
	DOMAttrsIterator(const DOMAttrsIterator& other) : attr_(other.attr_) {}

	/**
	 * @brief Move state of other iterator
	 * @param other Iterator to move
	 */
	DOMAttrsIterator(DOMAttrsIterator&& other) noexcept : attr_(std::move(other.attr_)) {}

	/**
	 * @brief Destruct iterator, no free or clean
	 */
	~DOMAttrsIterator() noexcept = default;

	/**
	 * @brief Copy state of other iterator
	 * @param other Iterator to copy
	 * @return *this
	 */
	inline DOMAttrsIterator& operator=(const DOMAttrsIterator& other) & {
		if (std::addressof(other) != this) {
			attr_ = other.attr_;
		}
		return *this;
	};

	/**
	 * @brief Move state of other iterator
	 * @param other Iterator to move
	 * @return *this
	 */
	inline DOMAttrsIterator& operator=(DOMAttrsIterator&& other) & noexcept {
		if (std::addressof(other) != this) {
			attr_ = std::move(other.attr_);
		}
		return *this;
	};

	/**
	 * @brief The attribute the iterator stands on.
	 * @note The reference is into the iterator itself, so it dies with it (or with
	 *       the next increment); copy the DOMAttrView out to keep it.
	 * @return View of the current attribute
	 */
	[[nodiscard]] const DOMAttrView& operator*() const;

	/**
	 * @brief The attribute the iterator stands on.
	 * @return Pointer to the view of the current attribute
	 */
	[[nodiscard]] const DOMAttrView* operator->() const;

	/**
	 * @brief Step to the previous attribute of the element.
	 * @pre The iterator must stand on an attribute: stepping back from the first
	 *      attribute, or from an end iterator, is undefined behavior.
	 * @return *this
	 */
	DOMAttrsIterator& operator--();

	/**
	 * @brief Step to the previous attribute of the element.
	 * @pre The iterator must stand on an attribute (@see operator--()).
	 * @return Copy of this iterator, still on the attribute it stood on
	 */
	DOMAttrsIterator operator--(int);

	/**
	 * @brief Step to the next attribute of the element.
	 * @pre The iterator must stand on an attribute: incrementing an end iterator is
	 *      undefined behavior. Past the last attribute it becomes the end iterator.
	 * @return *this
	 */
	DOMAttrsIterator& operator++();

	/**
	 * @brief Step to the next attribute of the element.
	 * @pre The iterator must stand on an attribute (@see operator++()).
	 * @return Copy of this iterator, still on the attribute it stood on
	 */
	DOMAttrsIterator operator++(int);

	/**
	 * @brief Compare iterators
	 * @param left  Left iterator
	 * @param right Right iterator
	 * @return true if both stand on the same attribute or both are end iterators,
	 *         false otherwise
	 */
	friend bool operator==(const DOMAttrsIterator& left, const DOMAttrsIterator& right);

private:
	DOMAttrView attr_ = nullptr;
};
} // namespace aniparse::html