/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/Expected.hpp"

#include <string_view>
#include <stdexcept>

typedef struct lxb_css_parser lxb_css_parser_t;
typedef struct lxb_css_selector_list lxb_css_selector_list_t;

namespace aniparse::html {

/**
 * @brief Error produced when a CSS selector string fails to parse.
 * @see CompiledSelector::compile, SelectorCompiler::compile
 */
struct SelectorParseError : std::runtime_error {
	using std::runtime_error::runtime_error;
};

/**
 * @brief A CSS selector parsed once into lexbor's form, ready to match.
 * Parsing (the fallible step) happens on construction; matching through
 * DOMElementView::query / HTMLDocument::query is infallible.
 * The compiled list owns its own memory and is not tied to the lifetime
 * of the parser that produced it.
 * @note Non-copyable, movable. Not thread safe to move while in use.
 */
class CompiledSelector {
	friend class SelectorCompiler;

public:
	/**
	 * @brief Parse a single selector using a private one-shot parser.
	 * When compiling many selectors prefer @ref SelectorCompiler, which
	 * reuses one parser for the whole batch.
	 * @param selector CSS selector string, e.g. ".card > a[href]"
	 * @return Compiled selector, or SelectorParseError if invalid
	 */
	[[nodiscard]] static expected<CompiledSelector, SelectorParseError> compile(std::string_view selector);

	/**
	 * @brief Take ownership from other selector.
	 * The other selector becomes invalid.
	 * @param other The selector to take ownership
	 */
	CompiledSelector(CompiledSelector&& other) noexcept;

	/**
	 * @brief Take ownership from other selector.
	 * The other selector becomes invalid.
	 * @param other The selector to take ownership
	 */
	CompiledSelector& operator=(CompiledSelector&& other) noexcept;

	CompiledSelector(const CompiledSelector&)            = delete;
	CompiledSelector& operator=(const CompiledSelector&) = delete;

	/**
	 * @brief Destroy and free the compiled selector list.
	 */
	~CompiledSelector();

	/**
	 * @return true if the selector holds a valid parsed list, false otherwise
	 */
	[[nodiscard]] operator bool() const noexcept;

	/**
	 * @brief Raw pointer to the lexbor selector list.
	 * @note Implementation defined, use it only inside the html layer.
	 * @return Raw pointer to the selector list
	 */
	[[nodiscard]] lxb_css_selector_list_t* get() const noexcept;

private:
	explicit CompiledSelector(lxb_css_selector_list_t* list) noexcept;

	lxb_css_selector_list_t* list_ = nullptr;
};

/**
 * @brief Compiles many CSS selectors reusing a single lexbor parser.
 * Cheaper than @ref CompiledSelector::compile in a loop, which spins up
 * a fresh parser on every call. Suited for building selector tables at
 * startup or in tests.
 * @note The compiler isn't thread safe.
 */
class SelectorCompiler {
public:
	/**
	 * @brief Allocate and create the underlying parser.
	 */
	SelectorCompiler();

	/**
	 * @brief Take ownership from other compiler.
	 * @param other The compiler to take ownership
	 */
	SelectorCompiler(SelectorCompiler&& other) noexcept;

	SelectorCompiler(const SelectorCompiler&) = delete;

	/**
	 * @brief Cleanup and free the underlying parser.
	 */
	~SelectorCompiler() noexcept;

	/**
	 * @brief Take ownership from other compiler.
	 * @param other The compiler to take ownership
	 */
	SelectorCompiler& operator=(SelectorCompiler&& other) noexcept;

	SelectorCompiler& operator=(const SelectorCompiler&) = delete;

	/**
	 * @brief Parse a selector, throwing on failure.
	 * Suited for static selector tables, where an invalid selector is a
	 * programming error that a test should catch loudly.
	 * @throw SelectorParseError if the selector is invalid
	 * @param selector CSS selector string
	 * @return Compiled selector
	 */
	[[nodiscard]] CompiledSelector compile(std::string_view selector);

	/**
	 * @brief Parse a selector without throwing.
	 * @param selector CSS selector string
	 * @return Compiled selector, or SelectorParseError if invalid
	 */
	[[nodiscard]] expected<CompiledSelector, SelectorParseError> try_compile(std::string_view selector);

private:
	lxb_css_parser_t* parser_ = nullptr;
};

} // namespace aniparse::html
