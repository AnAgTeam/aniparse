/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aniparse/utility/Attributes.hpp"

namespace aniparse {

/**
 * @brief A parsed and normalized URL (WHATWG rules, via lexbor).
 * Built once from a string, it owns the complete serialized URL and exposes its
 * components as cheap views. Parsers get one for routing (`valid_for_url` /
 * `suggest_getter` / `parse_url`) and inspect the @ref path without re-parsing.
 * @note Owning, copyable, and movable. Returned views are invalidated when
 * this object is moved from, assigned to, or destroyed.
 */
class ParsedUrl {
public:
	/**
	 * @brief Parse an absolute URL.
	 * @param url The URL string
	 * @return The parsed URL, or nullopt if lexbor rejects it (e.g. no scheme)
	 */
	[[nodiscard]] static std::optional<ParsedUrl> parse(std::string_view url);

	/**
	 * @brief Return the complete normalized URL.
	 * @return URL serialized by lexbor, including credentials, port, query, and fragment when present.
	 */
	[[nodiscard]] std::string_view href() const noexcept ANIPARSE_LIFETIMEBOUND { return href_; }

	/// Scheme without the trailing colon, e.g. "https".
	[[nodiscard]] std::string_view scheme() const noexcept ANIPARSE_LIFETIMEBOUND {
		return {href_.data() + scheme_.offset, scheme_.length};
	}
	/// Host (ASCII/IDNA), e.g. "api.example.com".
	[[nodiscard]] std::string_view host() const noexcept ANIPARSE_LIFETIMEBOUND {
		return {href_.data() + host_.offset, host_.length};
	}
	/// Path with its leading slash, e.g. "/manga/12345.html". Empty if none.
	[[nodiscard]] std::string_view path() const noexcept ANIPARSE_LIFETIMEBOUND {
		return {href_.data() + path_.offset, path_.length};
	}
	/// Query without its leading '?', e.g. "tab=info". Empty if none.
	[[nodiscard]] std::string_view query() const noexcept ANIPARSE_LIFETIMEBOUND {
		return {href_.data() + query_.offset, query_.length};
	}
	/// Fragment without its leading '#', e.g. "top". Empty if none.
	[[nodiscard]] std::string_view fragment() const noexcept ANIPARSE_LIFETIMEBOUND {
		return {href_.data() + fragment_.offset, fragment_.length};
	}

private:
	struct UrlPart {
		uint32_t offset = 0;
		uint32_t length = 0;
	};

	std::string href_;
	UrlPart    scheme_;
	UrlPart    host_;
	UrlPart    path_;
	UrlPart    query_;
	UrlPart    fragment_;
};

} // namespace aniparse
