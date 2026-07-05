/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace aniparse {

/**
 * @brief A parsed URL (WHATWG rules, via lexbor), owning its components.
 * Built once from a string, it holds each component so accessors return cheap
 * views. Parsers get one for routing (`valid_for_url` / `suggest_getter` /
 * `parse_url`) and inspect the @ref path without re-parsing the raw string.
 * @note Owning and movable; the returned views stay valid for the ParsedUrl's life.
 */
class ParsedUrl {
public:
	/**
	 * @brief Parse an absolute URL.
	 * @param url The URL string
	 * @return The parsed URL, or nullopt if lexbor rejects it (e.g. no scheme)
	 */
	[[nodiscard]] static std::optional<ParsedUrl> parse(std::string_view url);

	/// Scheme without the trailing colon, e.g. "https".
	[[nodiscard]] std::string_view scheme() const noexcept { return scheme_; }
	/// Host (ASCII/IDNA), e.g. "x8.h-chan.me".
	[[nodiscard]] std::string_view host() const noexcept { return host_; }
	/// Path with its leading slash, e.g. "/manga/12345.html". Empty if none.
	[[nodiscard]] std::string_view path() const noexcept { return path_; }
	/// Query with its leading '?', e.g. "?tab=info". Empty if none.
	[[nodiscard]] std::string_view query() const noexcept { return query_; }
	/// Fragment with its leading '#', e.g. "#top". Empty if none.
	[[nodiscard]] std::string_view fragment() const noexcept { return fragment_; }

private:
	std::string scheme_;
	std::string host_;
	std::string path_;
	std::string query_;
	std::string fragment_;
};

} // namespace aniparse
