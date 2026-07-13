/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/types/ParsedUrl.hpp"

#include <lexbor/url/url.h>

namespace aniparse {

namespace {

// lexbor serialize callback: append the emitted bytes to the std::string in ctx.
lxb_status_t append_to_string(const lxb_char_t* data, size_t length, void* ctx) {
	static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), length);
	return LXB_STATUS_OK;
}

} // namespace

std::optional<ParsedUrl> ParsedUrl::parse(std::string_view url) {
	lxb_url_parser_t parser;
	if (lxb_url_parser_init(&parser, nullptr) != LXB_STATUS_OK) {
		return std::nullopt;
	}

	lxb_url_t* parsed = lxb_url_parse(&parser,
	                                  nullptr,
	                                  reinterpret_cast<const lxb_char_t*>(url.data()),
	                                  url.size());
	// The parsed URL owns its own memory; destroying the parser (self only) keeps it.
	lxb_url_parser_destroy(&parser, false);

	if (parsed == nullptr) {
		return std::nullopt;
	}

	ParsedUrl view;
	lxb_url_serialize_scheme(parsed, append_to_string, &view.scheme_);
	lxb_url_serialize_host(lxb_url_host(parsed), append_to_string, &view.host_);
	lxb_url_serialize_path(lxb_url_path(parsed), append_to_string, &view.path_);
	lxb_url_serialize_query(parsed, append_to_string, &view.query_);
	lxb_url_serialize_fragment(parsed, append_to_string, &view.fragment_);

	lxb_url_memory_destroy(parsed);
	return view;
}

} // namespace aniparse
