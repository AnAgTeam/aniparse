/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#pragma once
#include "aniparse/html/HTMLDocument.hpp"

#include <stdexcept>
#include <lexbor/html/parser.h>

namespace aniparse::html {

	/**
	 * @brief Class representing HTML parsing error
	 * @see HTMLParser::parse
	 */
	struct HTMLParseError : std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	/**
	 * @brief Class for parsing HTML documents
	 * The parser isn't thread safe
	 */
	class HTMLParser {
	public:
		/**
		 * @brief Allocate and create parser object
		 */
		HTMLParser();

		HTMLParser(const HTMLParser& other) = delete;

		/**
		 * @brief Take ownership from other parser
		 * @param other The parser to take ownership
		 */
		HTMLParser(HTMLParser&& other) noexcept;

		/**
		 * @brief Cleanup and free document object
		 */
		~HTMLParser() noexcept;

		HTMLParser& operator=(const HTMLParser& other) = delete;

		/**
		 * @brief Take ownership from other parser
		 * @param other The parser to take ownership
		 */
		HTMLParser& operator=(HTMLParser&& other) noexcept;

		/**
		 * @brief Parse HTML document from string
		 * @param text Full HTML text to parse
		 * @note Returns valid HTMLDocument even if some tags
		 *       are invalid.
		 * @throw HTMLParseError if parsing failed: invalid DOCTYPE
		 * @return Parsed HTML document
		 */
		HTMLDocument parse(std::string_view text);

	private:
		lxb_html_parser_t* parser_ = nullptr;
	};
}