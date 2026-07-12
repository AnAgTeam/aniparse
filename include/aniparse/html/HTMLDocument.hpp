/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/html/DOMElement.hpp"

#include <string_view>
#include <optional>
#include <vector>

typedef struct lxb_html_document lxb_html_document_t;

namespace aniparse::html {

class CompiledSelector;

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
	[[nodiscard]] std::string_view title() const;

	/**
	 * @brief Get root HTML element \<HTML\>
	 * @return Root HTML element
	 */
	[[nodiscard]] DOMElementView as_element() const;

	/**
	 * @brief \<HEAD\> element inside root \<HTML\>
	 * @return \<HEAD\> element view
	 */
	[[nodiscard]] DOMElementView head() const;

	/**
	 * @brief \<BODY\> element inside root \<HTML\>
	 * @return \<BODY\> element view
	 */
	[[nodiscard]] DOMElementView body() const;

	/**
	 * @brief Find the first element in the document matching a CSS selector.
	 * @param selector Compiled CSS selector
	 * @return DOM element view if found, nullopt otherwise
	 */
	[[nodiscard]] std::optional<DOMElementView> query(const CompiledSelector& selector) const;

	/**
	 * @brief Find all elements in the document matching a CSS selector.
	 * @param selector Compiled CSS selector
	 * @return All found DOM element views, in document order
	 */
	[[nodiscard]] std::vector<DOMElementView> query_all(const CompiledSelector& selector) const;

private:
	lxb_html_document_t* document_ = nullptr;
};

/**
 * @brief Parse HTML document from string
 * Simply just shortcut to @ref HTMLParser::parse
 * @param text Full HTML text to parse
 * @note Returns valid HTMLDocument even if some tags
 *       are invalid.
 * @throw HTMLParseError if parsing failed: invalid DOCTYPE
 * @return Parsed HTML document
 */
extern HTMLDocument parse_html(std::string_view text);
} // namespace aniparse::html