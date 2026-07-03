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

} // namespace aniparse
