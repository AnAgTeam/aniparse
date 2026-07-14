/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <lexbor/dom/collection.h>

namespace aniparse::html {
/**
 * @brief Class for storing the HTML elements
 * walk results
 * @todo
 *
 * Owning move-only wrapper over a lexbor element collection — the container
 * lexbor's own search routines fill in. The elements it would hold are not owned
 * by it: they stay in the HTMLDocument that parsed them, which must outlive the
 * collection just as it must outlive a DOMElementView.
 *
 * @warning Unfinished, and unreachable as it stands: no constructor takes a
 *          collection (the only one is commented out) and the move constructor
 *          suppresses the implicit default one, so an instance cannot be created.
 *          Nothing in the library returns one either — the search API hands back
 *          std::vector<DOMElementView> (DOMElementView::query_all, find_all)
 *          instead.
 */
class DOMCollection {
public:
	//explicit DOMCollection(size_t size);
	/// @brief Non-copyable: the collection is owned, not shared.
	DOMCollection(const DOMCollection& other) = delete;

	/**
	 * @brief Transfer ownership of the collection from another wrapper.
	 * The other wrapper is left holding nothing.
	 * @param other The wrapper to take the collection from
	 */
	DOMCollection(DOMCollection&& other) noexcept;

	/**
	 * @brief Release the held collection, if any.
	 * The elements it referenced are untouched: they belong to their document.
	 */
	~DOMCollection() noexcept;

	/// @brief Non-copyable: the collection is owned, not shared.
	DOMCollection& operator=(const DOMCollection& other) = delete;

	/**
	 * @brief Transfer ownership of the collection from another wrapper.
	 * The other wrapper is left holding nothing.
	 * @param other The wrapper to take the collection from
	 * @return *this
	 */
	DOMCollection& operator=(DOMCollection&& other) noexcept;

private:
	lxb_dom_collection_t* collection_ = nullptr;
	size_t size                       = 0;
};
} // namespace aniparse::html