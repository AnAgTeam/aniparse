/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/JSParser.hpp"

#include <cctype>
#include <cstddef>
#include <boost/json.hpp>

namespace aniparse::html {

namespace {

bool is_ident_char(char c) {
	const unsigned char uc = static_cast<unsigned char>(c);
	return std::isalnum(uc) != 0 || c == '_' || c == '$';
}

bool is_space(char c) {
	return std::isspace(static_cast<unsigned char>(c)) != 0;
}

/// Scan a balanced object/array literal that opens at text[start].
/// Skips string/template literals and comments while matching brackets.
/// Returns the length of the literal (brackets included), or 0 if the
/// literal is not balanced within the text.
size_t scan_balanced(std::string_view text, size_t start) {
	std::string closers; // stack of the closing brackets we still expect

	for (size_t i = start; i < text.size(); ++i) {
		const char c = text[i];

		// String or template literal: skip to the matching quote.
		if (c == '"' || c == '\'' || c == '`') {
			const char quote = c;
			for (++i; i < text.size(); ++i) {
				if (text[i] == '\\') {
					++i; // skip the escaped character
					continue;
				}
				if (text[i] == quote) {
					break;
				}
			}
			if (i >= text.size()) {
				return 0; // unterminated string
			}
			continue;
		}

		// Comments.
		if (c == '/' && i + 1 < text.size()) {
			if (text[i + 1] == '/') {
				i += 2;
				while (i < text.size() && text[i] != '\n') {
					++i;
				}
				continue;
			}
			if (text[i + 1] == '*') {
				i += 2;
				while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) {
					++i;
				}
				if (i + 1 >= text.size()) {
					return 0; // unterminated comment
				}
				++i; // sit on '/', the loop step moves past it
				continue;
			}
		}

		// Brackets.
		if (c == '{' || c == '[') {
			closers.push_back(c == '{' ? '}' : ']');
		}
		else if (c == '}' || c == ']') {
			if (closers.empty() || closers.back() != c) {
				return 0; // mismatched bracket
			}
			closers.pop_back();
			if (closers.empty()) {
				return (i - start) + 1;
			}
		}
	}

	return 0; // never balanced
}

/// Scan the text for a top level object/array literal assigned to
/// variable_name and return the balanced literal (or an empty view).
std::string_view scan_json_var(std::string_view variable_name, std::string_view sv) {
	if (variable_name.empty()) {
		return {};
	}

	size_t pos = 0;

	while ((pos = sv.find(variable_name, pos)) != std::string_view::npos) {
		const size_t name_start = pos;
		const size_t name_end   = pos + variable_name.size();
		pos                     = name_end; // advance regardless of the outcome

		// Match the name as a whole identifier, not a substring of one.
		if (name_start > 0 && is_ident_char(sv[name_start - 1])) {
			continue;
		}
		if (name_end < sv.size() && is_ident_char(sv[name_end])) {
			continue;
		}

		size_t i = name_end;

		// Allow a quoted key: "name" or 'name'.
		if (name_start > 0 && (sv[name_start - 1] == '"' || sv[name_start - 1] == '\'')
		    && i < sv.size() && sv[i] == sv[name_start - 1]) {
			++i;
		}

		// Expect a '=' assignment or a ':' key separator.
		while (i < sv.size() && is_space(sv[i])) {
			++i;
		}
		if (i >= sv.size() || (sv[i] != '=' && sv[i] != ':')) {
			continue;
		}
		if (sv[i] == '=' && i + 1 < sv.size() && (sv[i + 1] == '=' || sv[i + 1] == '>')) {
			continue; // ==, ===, => are not assignments
		}
		++i;

		// Expect the value to open with an object or array literal.
		while (i < sv.size() && is_space(sv[i])) {
			++i;
		}
		if (i >= sv.size() || (sv[i] != '{' && sv[i] != '[')) {
			continue;
		}

		const size_t length = scan_balanced(sv, i);
		if (length > 0) {
			return sv.substr(i, length);
		}
		// Unbalanced from here; keep looking for another occurrence.
	}

	return {};
}

/// Transcode a JS object/array literal into JSON: single-quoted strings become
/// double-quoted (embedded double quotes escaped, \' unescaped), while
/// double-quoted strings and comments are copied through untouched.
std::string normalize_js_to_json(std::string_view src) {
	std::string out;
	out.reserve(src.size());

	size_t i = 0;
	while (i < src.size()) {
		const char c = src[i];

		if (c == '"') { // double-quoted string: copy verbatim
			out.push_back(c);
			++i;
			while (i < src.size()) {
				if (src[i] == '\\' && i + 1 < src.size()) {
					out.push_back(src[i]);
					out.push_back(src[i + 1]);
					i += 2;
					continue;
				}
				const char d = src[i++];
				out.push_back(d);
				if (d == '"') {
					break;
				}
			}
			continue;
		}

		if (c == '\'') { // single-quoted string: transcode to double-quoted
			out.push_back('"');
			++i;
			while (i < src.size()) {
				if (src[i] == '\\' && i + 1 < src.size()) {
					const char e = src[i + 1];
					if (e == '\'') {
						out.push_back('\''); // \' is not a JSON escape
					}
					else {
						out.push_back('\\');
						out.push_back(e);
					}
					i += 2;
					continue;
				}
				const char d = src[i++];
				if (d == '\'') {
					out.push_back('"'); // closing quote
					break;
				}
				if (d == '"') {
					out.push_back('\\'); // escape a double quote inside the string
				}
				out.push_back(d);
			}
			continue;
		}

		if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') { // line comment
			out.push_back(src[i]);
			out.push_back(src[i + 1]);
			i += 2;
			while (i < src.size() && src[i] != '\n') {
				out.push_back(src[i++]);
			}
			continue;
		}

		if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') { // block comment
			out.push_back(src[i]);
			out.push_back(src[i + 1]);
			i += 2;
			while (i < src.size()) {
				const char d = src[i++];
				out.push_back(d);
				if (d == '*' && i < src.size() && src[i] == '/') {
					out.push_back(src[i++]);
					break;
				}
			}
			continue;
		}

		out.push_back(c);
		++i;
	}

	return out;
}

} // namespace

std::string_view find_json_var(std::string_view variable_name, const std::string& text) {
	return scan_json_var(variable_name, text);
}

std::optional<boost::json::value> parse_json_var(std::string_view variable_name, std::string_view text) {
	const std::string_view slice = scan_json_var(variable_name, text);
	if (slice.empty()) {
		return std::nullopt;
	}

	const std::string normalized = normalize_js_to_json(slice);

	boost::json::parse_options options;
	options.allow_comments        = true;
	options.allow_trailing_commas = true;

	boost::system::error_code ec;
	boost::json::value value = boost::json::parse(normalized, ec, {}, options);
	if (ec) {
		return std::nullopt;
	}
	return value;
}

} // namespace aniparse::html
