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

	class DOMElementIterator;

	/**
	 * @brief Class for accessing DOM element attributes, name, etc.
	 * Supports iterating child elements. For walking @see DOMElementWalkIterator
	 * @note The class is non owning, so it must be alive with not "View" class
	 */
	class DOMElementView {
		friend class DOMNodeView;
		friend class DOMElementWalkIterator;
		friend class DOMElementIterator;

	public:

		using iterator_type = DOMElementIterator;

		/**
		 * @brief Make view of DOM element
		 * @param element DOM element
		 */
		DOMElementView(lxb_dom_element_t* element);

		/**
		 * @brief Make empty (invalid) DOM element view
		 * @note You can't use getters with invalid view.
		 */
		DOMElementView() = default;

		/**
		 * @brief Copy view of the same DOM element
		 * @param other DOM element
		 */
		DOMElementView(const DOMElementView& other) = default;

		/**
		 * @brief Copy view of the same DOM element
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
		 */
		DOMElementView& operator=(const DOMElementView& other) & noexcept = default;

		/**
		 * @brief Copy view of the same DOM element
		 * @param other DOM element
		 */
		DOMElementView& operator=(DOMElementView&& other) & noexcept = default;

		/**
		 * @return true if DOM element is valid, false otherwise
		 */
		[[nodiscard]] operator bool() const;

		/**
		 * @return View of the DOM element attributes
		 */
		[[nodiscard]] DOMElementAttrsView attributes() const;

		/**
		 * @brief Iterator to the start (first) of DOM element children
		 * @return DOM elements iterator
		 */
		[[nodiscard]] DOMElementIterator begin();

		/**
		 * @brief Iterator to the end of DOM element children.
		 * @note Taking value from it will be UB
		 * @return DOM elements iterator
		 */
		[[nodiscard]] DOMElementIterator end();

		/**
		 * @note Tag name always in upper case ("HTML", "DIV", etc.)
		 * @return DOM element tag/name, e.g. <html> => HTML
		 */
		[[nodiscard]] std::string_view tag_name() const;

		/**
		 * @brief Get the full class name of the element.
		 * This is just efficient shortcut to "class" attr, get_attr("class")
		 * The classes may be separated by whitespace characters
		 * @return DOM element class
		 */
		[[nodiscard]] std::string_view class_name() const;

		/**
		 * @brief Get the id name of the element.
		 * This is just efficient shortcut to "id" attr, get_attr("id")
		 * @return DOM element id
		 */
		[[nodiscard]] std::string_view id() const;

		/**
		 * @brief Check if the element contains class.
		 * Checks with respect to whitespaces.
		 * @param name Class name to check
		 * @return true if the class is found, false otherwise
		 */
		[[nodiscard]] bool contains_class(std::string_view name) const;

		/**
		 * @brief Find first element with tag
		 * @note Tag finding is case sensitive! (must be uppercase), @ref tag_name
		 * @param tag DOM element tag name
		 * @return DOM element view if found, nullopt otherwise
		 */
		[[nodiscard]] std::optional<DOMElementView> find(std::string_view tag) const;

		/**
		 * @brief Find first element with attribute and value
		 * @param attr DOM attribute name
		 * @param value DOM Attrubute value
		 * @param ignore_class_whitespace Only for "attr" == "class".
		 *        If true, then it checks if element contains "value" class,
		 *        Otherwise, checks if element class exactly the same
		 * @return DOM element view if found, nullopt otherwise
		 */
		[[nodiscard]] std::optional<DOMElementView> find(
			std::string_view attr,
			std::string_view value,
			bool ignore_class_whitespaces = true) const;

		/**
		 * @brief Find all elements with tag
		 * @note Tag finding is case sensitive! (must be uppercase)
		 * @param tag DOM element tag name
		 * @return All found DOM element views
		 */
		[[nodiscard]] std::vector<DOMElementView> find_all(std::string_view tag) const;

		/**
		 * @brief Find all elements with attribute and value
		 * @param attr DOM attribute name
		 * @param value DOM Attrubute value
		 * @param ignore_class_whitespace Only for "attr" == "class".
		 *        If true, then it checks if element contains "value" class,
		 *        Otherwise, checks if element class exactly the same
		 * @return All found DOM element views
		 */
		[[nodiscard]] std::vector<DOMElementView> find_all(
			std::string_view attr,
			std::string_view value,
			bool ignore_class_whitespaces = true) const;

		/**
		 * @brief Find DOM element attribute with name.
		 * @note to access "class" or "id" it is better to use @ref class_name() and @ref id()
		 * @param name DOM attribute name
		 * @return DOM attribute view if found, nullopt otherwise
		 */
		[[nodiscard]] std::optional<DOMAttrView> find_attr(std::string_view name) const;

		/**
		 * @brief Get DOM element attribute value with name.
		 * @note to access "class" or "id" it is better to use @ref class_name() and @ref id()
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
		 * @return Text of the element or empty string
		 */
		[[nodiscard]] std::string_view content_text() const;

		/**
		 * @brief Get escaped text of element.
		 * Walks though all chilren of the element.
		 * Ignores all new line characters (with space),
		 * and removes leading and trailing spaces.
		 * Interprets <BR> element as new line ('\n').
		 * @return Text of the element or empty string
		 */
		[[nodiscard]] std::string text() const;

		//std::string_view to_string() const;

		/**
		 * @brief Get raw pointer to the DOM element
		 *        implementation defined struct.
		 * @note Use it only if you *have to*. This
		 *       pointer should be implementation defined
		 * @return Raw pointer to the DOM element
		 */
		[[nodiscard]] lxb_dom_element_t* get() const;

	private:
		lxb_dom_element_t* element_ = nullptr;
	};

	/**
	 * @brief Class for iterating through all element children.
	 * Supports bidirectional iterating
	 * @note Becomes invalid when reaches start or end
	 */
	class DOMElementIterator {
	public:
		using value_type = DOMElementView;
		using difference_type = std::ptrdiff_t;
		using pointer = const DOMAttrView*;
		using reference = const DOMAttrView&;
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
		 * @brief Copy state of other iterator
		 * @param other Iterator to copy
		 */
		DOMElementIterator(DOMElementIterator&& other) noexcept = default;

		/**
		 * @brief Destruct iterator
		 */
		~DOMElementIterator() noexcept = default;

		/**
		 * @brief Copy state of other iterator
		 * @param other Iterator to copy
		 */
		DOMElementIterator& operator=(const DOMElementIterator& other) & = default;

		/**
		 * @brief Copy state of other iterator
		 * @param other Iterator to copy
		 */
		DOMElementIterator& operator=(DOMElementIterator&& other) & noexcept = default;

		/**
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
		[[nodiscard]] friend bool operator==(const DOMElementIterator& left, const DOMElementIterator& right);

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
	 */
	class DOMElementWalkIterator {
	public:
		using value_type = DOMElementView;
		using difference_type = std::ptrdiff_t;
		using pointer = const DOMElementView*;
		using reference = const DOMElementView&;
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
		 * @param element DOM element
		 */
		DOMElementWalkIterator() noexcept = default;

		/**
		 * @brief Copy state of other iterator
		 * @param other Iterator to copy
		 */
		DOMElementWalkIterator(const DOMElementWalkIterator& other) noexcept = default;

		/**
		 * @brief Copy state of other iterator
		 * @param other Iterator to copy
		 */
		DOMElementWalkIterator(DOMElementWalkIterator&& other) noexcept = default;

		/**
		 * @brief Destruct iterator
		 */
		~DOMElementWalkIterator() noexcept = default;

		/**
		 * @brief Copy state of other iterator
		 * @param other Iterator to copy
		 */
		DOMElementWalkIterator& operator=(const DOMElementWalkIterator& other) & = default;

		/**
		 * @brief Copy state of other iterator
		 * @param other Iterator to copy
		 */
		DOMElementWalkIterator& operator=(DOMElementWalkIterator&& other) & noexcept = default;

		/**
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
		[[nodiscard]] friend bool operator==(const DOMElementWalkIterator& left, const DOMElementWalkIterator& right) noexcept;

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

	[[nodiscard]] inline DOMElementWalkIterator begin(DOMElementWalkIterator iter) noexcept {
		return iter;
	}

	[[nodiscard]] inline DOMElementWalkIterator end(const DOMElementWalkIterator&) noexcept {
		return {};
	}

	/**
	 * @brief Class for storing and accessing DOM element.
	 * Can be used to get element attributes, name, etc.
	 * Supports iterating child elements. For walking @see DOMElementWalkIterator
	 * @todo
	 */
	class DOMElement {
	public:
		DOMElement(lxb_dom_element_t* element);

		DOMElement();
		DOMElement(const DOMElement& other) = delete;
		DOMElement(DOMElement&& other) noexcept;
		~DOMElement();

		DOMElement& operator=(const DOMElement& other) = delete;
		DOMElement& operator=(DOMElement&& other) noexcept;

		operator DOMElementView();

		lxb_dom_element_t* get();
		const lxb_dom_element_t* get() const;

	private:
		lxb_dom_element_t* element_ = nullptr;
	};
}