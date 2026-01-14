#pragma once
#include "aniparse/html/DOMElement.hpp"

#include <string_view>
#include <optional>

typedef struct lxb_html_document lxb_html_document_t;

namespace aniparse::html {

	/**
	 * @brief Class for storing HTML document and all
	 * of its child members
	 * To parse @see HTMLParser::parse
	 */
	class HTMLDocument {
	public:
		/**
		 * @brief Create document from raw pointer.
		 * Takes ownership of pointer
		 * @param document Lexbor document raw pointer
		 */
		HTMLDocument(lxb_html_document_t* document);

		HTMLDocument(const HTMLDocument& other) = delete;

		/**
		 * @brief Transfer ownership from other document.
		 * The other document becomes invalid
		 * @param other The document to take ownership
		 */
		HTMLDocument(HTMLDocument&& other) noexcept;
		//explicit HTMLDocument(std::string_view text);

		/**
		 * @brief Destroy and cleanup HTML document data
		 */
		~HTMLDocument();

		HTMLDocument& operator=(const HTMLDocument& other) = delete;

		/**
		 * @brief Transfer ownership from other document.
		 * The other document becomes invalid
		 * @param other The document to take ownership
		 */
		HTMLDocument& operator=(HTMLDocument&& other) noexcept;

		/**
		 * @brief Get HTML document title
		 * @return HTML document title, or empty string if no title
		 */
		std::string_view title() const;

		/**
		 * @brief Get root HTML element <HTML>
		 * @return Root HTML element
		 */
		DOMElementView as_element() const;

		/**
		 * @brief <HEAD> element inside root <HTML>
		 * @return <HEAD> element view
		 */
		DOMElementView head() const;

		/**
		 * @brief <BODY> element inside root <HTML>
		 * @return <BODY> element view
		 */
		DOMElementView body() const;

		std::optional<DOMElementView> find_first_by_class(std::string_view name) const;

	private:
		lxb_html_document_t* document_ = nullptr;
	};
}