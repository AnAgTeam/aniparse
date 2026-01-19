/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#pragma once
#include <string_view>
#include <optional>

typedef struct lxb_dom_attr lxb_dom_attr_t;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace aniparse::html {
	/**
	 * @brief Class for accessing the DOM element attribute
	 * @note The class is non owning, so it must be alive with not "View" class
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
		 */
		DOMAttrView& operator=(const DOMAttrView& other) & = default;

		/**
		 * @brief Copy view of the same attribute
		 * @param other DOM element attribute
		 */
		DOMAttrView& operator=(DOMAttrView&& other) & = default;

		/**
		 * @return Name of the attribute
		 */
		[[nodiscard]] std::string_view name() const;

		/**
		 * @return Value of the attribute
		 */
		[[nodiscard]] std::string_view value() const;

	private:
		lxb_dom_attr_t* attr_ = nullptr;
	};

	/**
	 * @brief Class for accessing all the DOM element attributes
	 * @note The class is non owning, so it must be alive with not "View" class
	 */
	class DOMElementAttrsView {
		friend class DOMElementView;
		friend class DOMElement;

	public:

		DOMElementAttrsView(lxb_dom_element_t* element);

		DOMElementAttrsView() = default;
		DOMElementAttrsView(const DOMElementAttrsView& other) = default;
		DOMElementAttrsView(DOMElementAttrsView&& other) = default;
		~DOMElementAttrsView() = default;

		[[nodiscard]] DOMAttrsIterator begin();
		[[nodiscard]] DOMAttrsIterator end();

	private:
		lxb_dom_element_t* element_ = nullptr;
	};

	/**
	 * @brief Class for iterating through DOM element attributes
	 * @note Becomes invalid when reaches start or end
	 */
	class DOMAttrsIterator {
	public:
		using value_type = DOMElementView;
		using difference_type = std::ptrdiff_t;
		using pointer = const DOMAttrView*;
		using reference = const DOMAttrView&;
		using iterator_category = std::bidirectional_iterator_tag;

		DOMAttrsIterator(lxb_dom_attr_t* attr);

		DOMAttrsIterator() noexcept = default;
		DOMAttrsIterator(const DOMAttrsIterator& other) : attr_(other.attr_) {}
		DOMAttrsIterator(DOMAttrsIterator&& other) noexcept : attr_(std::move(other.attr_)) {}
		~DOMAttrsIterator() noexcept = default;

		inline DOMAttrsIterator& operator=(const DOMAttrsIterator& other)& {
			if (std::addressof(other) != this) {
				attr_ = other.attr_;
			}
			return *this;
		};

		inline DOMAttrsIterator& operator=(DOMAttrsIterator&& other) & noexcept {
			if (std::addressof(other) != this) {
				attr_ = std::move(other.attr_);
			}
			return *this;
		};

		[[nodiscard]] const DOMAttrView& operator*() const;
		[[nodiscard]] const DOMAttrView* operator->() const;

		DOMAttrsIterator& operator--();
		DOMAttrsIterator operator--(int);

		DOMAttrsIterator& operator++();
		DOMAttrsIterator operator++(int);

		[[nodiscard]] friend bool operator==(const DOMAttrsIterator& left, const DOMAttrsIterator& right);

	private:
		DOMAttrView attr_ = nullptr;
	};
}