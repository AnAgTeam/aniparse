/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/types/ParsedUrl.hpp"

#include <limits>

#include <lexbor/url/url.h>

namespace aniparse {

namespace {

// lexbor serialize callback: append the emitted bytes to the std::string in ctx.
lxb_status_t append_to_string(const lxb_char_t* data, size_t length, void* ctx) {
	static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), length);
	return LXB_STATUS_OK;
}

lxb_status_t count_serialized_bytes(const lxb_char_t*, size_t length, void* ctx) {
	*static_cast<size_t*>(ctx) += length;
	return LXB_STATUS_OK;
}

template <typename Serialize>
std::optional<uint32_t> serialized_length(Serialize&& serialize) {
	size_t length = 0;
	if (serialize(count_serialized_bytes, &length) != LXB_STATUS_OK
	    || length > std::numeric_limits<uint32_t>::max())
	{
		return std::nullopt;
	}

	return static_cast<uint32_t>(length);
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

	ParsedUrl result;
	if (lxb_url_serialize(parsed, append_to_string, &result.href_, false) != LXB_STATUS_OK
	    || result.href_.size() > std::numeric_limits<uint32_t>::max())
	{
		lxb_url_memory_destroy(parsed);
		return std::nullopt;
	}

	const auto scheme_length = serialized_length(
	    [&](auto callback, auto context) { return lxb_url_serialize_scheme(parsed, callback, context); });
	const auto host_length = serialized_length(
	    [&](auto callback, auto context) { return lxb_url_serialize_host(lxb_url_host(parsed), callback, context); });
	const auto path_length = serialized_length(
	    [&](auto callback, auto context) { return lxb_url_serialize_path(lxb_url_path(parsed), callback, context); });
	const auto query_length = serialized_length(
	    [&](auto callback, auto context) { return lxb_url_serialize_query(parsed, callback, context); });
	const auto fragment_length = serialized_length(
	    [&](auto callback, auto context) { return lxb_url_serialize_fragment(parsed, callback, context); });

	if (!scheme_length || !host_length || !path_length || !query_length || !fragment_length) {
		lxb_url_memory_destroy(parsed);
		return std::nullopt;
	}

	const auto has_query    = lxb_url_query(parsed)->data != nullptr;
	const auto has_fragment = lxb_url_fragment(parsed)->data != nullptr;
	const auto path_offset = static_cast<uint32_t>(result.href_.size() - *path_length - *query_length
	                                               - *fragment_length - (has_query ? 1 : 0)
	                                               - (has_fragment ? 1 : 0));

	result.scheme_ = {0, *scheme_length};
	result.path_ = {path_offset, *path_length};
	result.query_ = {static_cast<uint32_t>(path_offset + *path_length + (has_query ? 1 : 0)), *query_length};
	result.fragment_ = {static_cast<uint32_t>(result.query_.offset + *query_length + (has_fragment ? 1 : 0)),
	                    *fragment_length};

	if (lxb_url_host(parsed)->type != LXB_URL_HOST_TYPE__UNDEF) {
		auto host_offset = static_cast<uint32_t>(*scheme_length + 3); // ':' + "//"
		if (parsed->username.length != 0 || parsed->password.length != 0) {
			host_offset += static_cast<uint32_t>(parsed->username.length + parsed->password.length + 1);
			if (parsed->password.length != 0) {
				++host_offset;
			}
		}
		result.host_ = {host_offset, *host_length};
	}

	lxb_url_memory_destroy(parsed);
	return result;
}

} // namespace aniparse
