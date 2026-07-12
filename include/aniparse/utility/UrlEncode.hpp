/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <string>
#include <string_view>

/**
 * @file
 * Percent-encoding (RFC 3986) for the places a parser builds a URL or a form
 * body by hand. Note the asymmetry: query parameters set through UrlParameters
 * are encoded by the transport on send, so they must be left raw — this is for
 * everything the transport does not encode for you.
 */

namespace aniparse {

/**
 * @brief Percent-encode a string per RFC 3986.
 * Unreserved characters (A-Z a-z 0-9 - . _ ~) pass through; every other byte
 * becomes %XX with uppercase hex (space -> %20, not '+'). Use it to build
 * application/x-www-form-urlencoded bodies or to escape a value placed into a
 * URL by hand.
 * @note URL query parameters set via @ref aniparse::UrlParameters are encoded by the
 *       transport on send and must NOT be pre-encoded with this (double-encoding).
 * @param value Raw value
 * @return Percent-encoded value
 */
inline std::string url_encode(std::string_view value) {
	static constexpr char hex[] = "0123456789ABCDEF";
	std::string out;
	out.reserve(value.size());
	for (unsigned char c : value) {
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
		    c == '-' || c == '.' || c == '_' || c == '~') {
			out.push_back(static_cast<char>(c));
		} else {
			out.push_back('%');
			out.push_back(hex[c >> 4]);
			out.push_back(hex[c & 0x0F]);
		}
	}
	return out;
}

} // namespace aniparse
