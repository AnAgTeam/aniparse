/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/CompiledSelector.hpp"
#include "aniparse/utility/Format.hpp"

#include <utility>
#include <memory>
#include <lexbor/css/css.h>

namespace aniparse::html {

namespace {

/// Parse a selector with the given parser. Returns nullptr on failure,
/// freeing any partially built list.
lxb_css_selector_list_t* parse_selector(lxb_css_parser_t* parser, std::string_view selector) {
	if (parser == nullptr) {
		return nullptr;
	}

	lxb_css_selector_list_t* list = lxb_css_selectors_parse(
	    parser,
	    reinterpret_cast<const lxb_char_t*>(selector.data()),
	    selector.size());

	// A fresh parser owns no memory, so parse allocates a dedicated memory
	// object for the returned list. The list therefore outlives the parser.
	if (list == nullptr || parser->status != LXB_STATUS_OK) {
		if (list != nullptr) {
			lxb_css_selector_list_destroy_memory(list);
		}
		return nullptr;
	}
	return list;
}

} // namespace

CompiledSelector::CompiledSelector(lxb_css_selector_list_t* list) noexcept
    : list_(list) {
}

CompiledSelector::CompiledSelector(CompiledSelector&& other) noexcept
    : list_(std::exchange(other.list_, nullptr)) {
}

CompiledSelector& CompiledSelector::operator=(CompiledSelector&& other) noexcept {
	if (std::addressof(other) != this) {
		if (list_ != nullptr) {
			lxb_css_selector_list_destroy_memory(list_);
		}
		list_ = std::exchange(other.list_, nullptr);
	}
	return *this;
}

CompiledSelector::~CompiledSelector() {
	if (list_ != nullptr) {
		lxb_css_selector_list_destroy_memory(list_);
	}
}

CompiledSelector::operator bool() const noexcept {
	return list_ != nullptr;
}

lxb_css_selector_list_t* CompiledSelector::get() const noexcept {
	return list_;
}

expected<CompiledSelector, SelectorParseError> CompiledSelector::compile(std::string_view selector) {
	SelectorCompiler compiler;
	return compiler.try_compile(selector);
}

SelectorCompiler::SelectorCompiler() {
	parser_ = lxb_css_parser_create();
	if (parser_ != nullptr && lxb_css_parser_init(parser_, nullptr) != LXB_STATUS_OK) {
		parser_ = lxb_css_parser_destroy(parser_, true);
	}
}

SelectorCompiler::SelectorCompiler(SelectorCompiler&& other) noexcept
    : parser_(std::exchange(other.parser_, nullptr)) {
}

SelectorCompiler& SelectorCompiler::operator=(SelectorCompiler&& other) noexcept {
	if (std::addressof(other) != this) {
		if (parser_ != nullptr) {
			lxb_css_parser_destroy(parser_, true);
		}
		parser_ = std::exchange(other.parser_, nullptr);
	}
	return *this;
}

SelectorCompiler::~SelectorCompiler() noexcept {
	if (parser_ != nullptr) {
		lxb_css_parser_destroy(parser_, true);
	}
}

CompiledSelector SelectorCompiler::compile(std::string_view selector) {
	expected<CompiledSelector, SelectorParseError> result = try_compile(selector);
	if (!result) {
		throw result.error();
	}
	return std::move(result).value();
}

expected<CompiledSelector, SelectorParseError> SelectorCompiler::try_compile(std::string_view selector) {
	lxb_css_selector_list_t* list = parse_selector(parser_, selector);
	if (list == nullptr) {
		return unexpected(SelectorParseError(format("invalid CSS selector: {}", selector)));
	}
	return CompiledSelector(list);
}

} // namespace aniparse::html
