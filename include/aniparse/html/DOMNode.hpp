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

	/**
	 * @brief Class for accessing DOM node like element and text
	 * Supports iterating child nodes. For walking @see DOMNodetWalkIterator
	 * @note The class is non owning, so it must be alive with not "View" class
	 */
	class DOMNodeView {
		friend class DOMNodeWalkIterator;
		friend class DOMNodeIterator;
		friend class DOMElementView;
		friend class DOMElementIterator;
		friend class DOMElementIterator;

	public:

		using iterator_type = DOMNodeIterator;

		/**
		 * @brief Make view of DOM node
		 * @param node DOM node
		 */
		DOMNodeView(lxb_dom_node_t* node) noexcept;

		/**
		 * @brief Make view of DOM element node
		 * @param node DOM node
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
		 */
		DOMNodeView& operator=(const DOMNodeView& other) & noexcept = default;

		/**
		 * @brief Copy view of the same DOM node
		 * @param other DOM node
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

		[[nodiscard]] bool is_element() const noexcept;

		[[nodiscard]] bool is_text() const noexcept;

		DOMElementView as_element() const;

		std::string_view as_text() const;

		DOMNodeView first_child() const noexcept;

		DOMNodeView parent() const noexcept;

		DOMNodeView prev() const noexcept;

		DOMNodeView next() const noexcept;

		[[nodiscard]] friend bool operator==(const DOMNodeView& left, const DOMNodeView& right) noexcept;

	private:
		lxb_dom_node_t* node_ = nullptr;
	};

	/**
	 * @brief Class for *recursively* iterating (walking)
	 *        through all the DOM node children.
	 * @note Becomes invalid when reaches end
	 *       or node is destroyed
	 */
	class DOMNodeWalkIterator {
	public:
		using value_type = DOMNodeView;
		using difference_type = std::ptrdiff_t;
		using pointer = const DOMNodeView*;
		using reference = const DOMNodeView&;
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
		 */
		DOMNodeWalkIterator& operator=(const DOMNodeWalkIterator& other) & noexcept = default;

		/**
		 * @brief Copy state of other iterator
		 * @param other Iterator to copy
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
		[[nodiscard]] friend bool operator==(const DOMNodeWalkIterator& left, const DOMNodeWalkIterator& right) noexcept;

	private:

		/**
		 * @brief Walk to next node
		 */
		void next();

		DOMNodeView root_ = nullptr;
		DOMNodeView node_ = nullptr;
	};

	[[nodiscard]] inline DOMNodeWalkIterator begin(DOMNodeWalkIterator iter) {
		return iter;
	}

	[[nodiscard]] inline DOMNodeWalkIterator end(const DOMNodeWalkIterator&) {
		return {};
	}
}