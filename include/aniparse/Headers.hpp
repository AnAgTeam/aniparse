/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <string_view>

namespace aniparse {

/**
 * @brief Case-insensitive comparator for HTTP header names.
 * Header field names are ASCII tokens (RFC 7230), so ASCII lowercasing is enough.
 */
struct CaseInsensitiveLess {
	using is_transparent = void;

	static char to_lower(char c) noexcept {
		return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	}

	bool operator()(std::string_view left, std::string_view right) const noexcept {
		return std::lexicographical_compare(
		    left.begin(), left.end(), right.begin(), right.end(),
		    [](char l, char r) { return to_lower(l) < to_lower(r); });
	}
};

/**
 * @brief HTTP headers as a case-insensitive name -> value map.
 * Deduplicates by header name; merge order decides precedence.
 */
using Headers = std::map<std::string, std::string, CaseInsensitiveLess>;

/**
 * @brief Join headers into "Name: Value" lines for the transport layer.
 * @param headers Headers to serialize
 * @return List of header lines
 */
inline std::list<std::string> to_header_lines(const Headers& headers) {
	std::list<std::string> lines;
	for (const auto& [name, value] : headers) {
		lines.push_back(name + ": " + value);
	}
	return lines;
}

/**
 * @brief Parse a raw response header block into a Headers map.
 * Inverse of @ref to_header_lines. Accepts a block of CRLF- or LF-delimited
 * "Name: Value" lines (a trailing CR is tolerated). Lines with no colon (a
 * status line, a blank separator, an obsolete fold continuation) are skipped;
 * optional whitespace around the value is trimmed. Duplicate names collapse
 * last-wins, matching the case-insensitive map (Set-Cookie is handled by the
 * cookie jar, not here).
 * @param block Raw header block
 * @return Parsed headers
 */
inline Headers parse_header_block(std::string_view block) {
	Headers headers;
	size_t pos = 0;
	while (pos < block.size()) {
		size_t eol = block.find('\n', pos);
		std::string_view line = block.substr(pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
		pos = eol == std::string_view::npos ? block.size() : eol + 1;

		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}
		size_t colon = line.find(':');
		if (colon == std::string_view::npos) {
			continue;
		}
		std::string_view name  = line.substr(0, colon);
		std::string_view value = line.substr(colon + 1);
		if (name.empty()) {
			continue;
		}

		constexpr std::string_view ows = " \t";
		value.remove_prefix(std::min(value.find_first_not_of(ows), value.size()));
		if (auto last = value.find_last_not_of(ows); last != std::string_view::npos) {
			value = value.substr(0, last + 1);
		} else {
			value = {};
		}

		headers[std::string(name)] = std::string(value);
	}
	return headers;
}

} // namespace aniparse
