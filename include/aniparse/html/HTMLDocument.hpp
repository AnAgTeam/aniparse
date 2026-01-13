#pragma once
#include <string_view>
#include <optional>
#include <lexbor/html/parser.h>

namespace aniparse {

	class DOMElement;
	class DOMElementView;

	class DOMNode;
	class DOMNodeView;

	/**
	 * @brief Class for storing HTML document and all
	 * of its child members
	 */
	class HTMLDocument {
	public:
		HTMLDocument(lxb_html_document* document);

		HTMLDocument(const HTMLDocument& other) = delete;
		HTMLDocument(HTMLDocument&& other) noexcept;
		//explicit HTMLDocument(std::string_view text);
		~HTMLDocument();

		HTMLDocument& operator=(const HTMLDocument& other) = delete;
		HTMLDocument& operator=(HTMLDocument&& other) noexcept;

		std::string_view title() const;

		DOMElementView as_element() const;

		std::optional<DOMElementView> find_first_by_class(std::string_view name) const;

	private:
		lxb_html_document* document_ = nullptr;
	};

	/**
	 * @brief Class for parsing HTML documents
	 */
	class HTMLParser {
	public:
		HTMLParser();
		HTMLParser(const HTMLParser& other) = delete;
		HTMLParser(HTMLParser&& other) noexcept;
		~HTMLParser() noexcept;

		HTMLParser& operator=(const HTMLParser& other) = delete;
		HTMLParser& operator=(HTMLParser&& other) noexcept;

		HTMLDocument parse(std::string_view text);

	private:
		lxb_html_parser_t* parser_ = nullptr;
	};

	/**
	 * @brief Class for storing the HTML elements
	 * walk results
	 */
	class DOMCollection {
	public:
		//explicit DOMCollection(size_t size);
		DOMCollection(const DOMCollection& other) = delete;
		DOMCollection(DOMCollection&& other) noexcept;
		~DOMCollection() noexcept;

		DOMCollection& operator=(const DOMCollection& other) = delete;
		DOMCollection& operator=(DOMCollection&& other) noexcept;

	private:
		lxb_dom_collection_t* collection_ = nullptr;
		size_t size = 0;
	};

	/**
	 * @brief Class for accessing the DOM element attribute
	 * @note The class is non owning, so it must be alive with not "View" class
	 */
	class DOMAttrView {
		friend class DOMAttrsIterator;
		friend bool operator==(const DOMAttrsIterator& left, const DOMAttrsIterator& right);

	public:
		DOMAttrView(lxb_dom_attr_t* attr);

		DOMAttrView() = default;
		DOMAttrView(const DOMAttrView& other) = default;
		DOMAttrView(DOMAttrView&& other) = default;
		~DOMAttrView() noexcept = default;

		DOMAttrView& operator=(const DOMAttrView& other) & = default;
		DOMAttrView& operator=(DOMAttrView&& other) & = default;

		std::string_view name() const;
		std::string_view value() const;

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

		DOMAttrsIterator begin();
		DOMAttrsIterator end();

	private:
		lxb_dom_element_t* element_ = nullptr;
	};

	/**
	 * @brief Class for accessing DOM element attributes, name, etc.
	 * Supports iterating child elements. For walking @see DOMElementWalkIterator
	 * @note The class is non owning, so it must be alive with not "View" class
	 */
	class DOMElementView {
		friend class DOMElementWalkIterator;
		friend class DOMElementIterator;

	public:

		using iterator_type = DOMElementIterator;

		DOMElementView(lxb_dom_element_t* element);

		DOMElementView() = default;
		DOMElementView(const DOMElementView& other) = default;
		DOMElementView(DOMElementView&& other) = default;
		~DOMElementView() noexcept = default;

		//std::optional<DOMElementView> find_first_by_class(std::string_view name) const;

		DOMElementView& operator=(const DOMElementView& other) & noexcept = default;
		DOMElementView& operator=(DOMElementView&& other) & noexcept = default;

		operator bool() const;

		//explicit operator DOMElement();

		DOMElementAttrsView attributes() const;

		DOMElementIterator begin();
		DOMElementIterator end();

		/**
		 * @todo
		 * @note Tag name always in upper case ("HTML", "DIV", etc.)
		 */
		std::string_view tag_name() const;
		std::string_view class_name() const;
		std::string_view id() const;
		bool contains_class(std::string_view name) const;

		/**
		 * @todo
		 * @note to access "class" or "id" it is better to use @ref class_name() and @ref id()
		 */
		std::optional<DOMAttrView> find_attr(std::string_view name) const;

		std::optional<std::string_view> get_attr(std::string_view name) const;

		lxb_dom_element_t* get() const;

	private:
		lxb_dom_element_t* element_ = nullptr;
	};

	/**
	 * @brief Class for iterating through DOM element attributes
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

		const DOMAttrView& operator*() const;
		const DOMAttrView* operator->() const;

		DOMAttrsIterator& operator--();
		DOMAttrsIterator& operator++();

		friend bool operator==(const DOMAttrsIterator& left, const DOMAttrsIterator& right);

	private:
		DOMAttrView attr_ = nullptr;
	};

	/**
	 * @brief Class for iterating through all element children
	 * @note Becomes invalid when reaches start or end
	 */
	class DOMElementIterator {
	public:
		using value_type = DOMElementView;
		using difference_type = std::ptrdiff_t;
		using pointer = const DOMAttrView*;
		using reference = const DOMAttrView&;
		using iterator_category = std::bidirectional_iterator_tag;

		DOMElementIterator(lxb_dom_element_t* element);

		DOMElementIterator() noexcept = default;

		const DOMElementView& operator*() const;
		const DOMElementView* operator->() const;

		DOMElementIterator& operator--();
		DOMElementIterator operator--(int);

		DOMElementIterator& operator++();
		DOMElementIterator operator++(int);

		friend bool operator==(const DOMElementIterator& left, const DOMElementIterator& right);

	private:

		void iterate_until_element();

		lxb_dom_node_t* node_ = nullptr;
		DOMElementView node_view_;
	};

	/**
	 * @brief Class for *recursively* iterating (walking)
	 * through DOM element children
	 * @note Becomes invalid when reaches end
	 */
	class DOMElementWalkIterator {
	public:
		using value_type = DOMElementView;
		using difference_type = std::ptrdiff_t;
		using pointer = const DOMElementView*;
		using reference = const DOMElementView&;
		using iterator_category = std::input_iterator_tag;

		DOMElementWalkIterator() noexcept = default;
		explicit DOMElementWalkIterator(const DOMElementView& element);

		const DOMElementView& operator*() const;
		const DOMElementView* operator->() const;

		DOMElementWalkIterator& operator++();

		friend bool operator==(const DOMElementWalkIterator& left, const DOMElementWalkIterator& right) noexcept;

	private:

		void next();
		void walk_until_element();

		lxb_dom_node_t* root_ = nullptr;
		lxb_dom_node_t* node_ = nullptr;
		DOMElementView node_view_;
	};


	/**
	 * @brief Class for storing and accessing DOM element
	 * Can be used to get element attributes, name, etc.
	 * Supports iterating child elements. For walking @see DOMElementWalkIterator
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