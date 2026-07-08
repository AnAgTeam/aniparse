/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <algorithm>
#include <initializer_list>
#include <list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
 * @brief HTTP headers as an insertion-ordered list of name/value pairs.
 *
 * Backed by a list rather than a map so it keeps two things a sorted map loses:
 * the order in which headers were added (the order they go on the wire) and
 * repeated names (several Set-Cookie lines coexist instead of collapsing). Name
 * lookup is case-insensitive; the stored casing is preserved as written.
 *
 * Two access modes: by-name convenience (get/at/contains) returns the first
 * match and suits single-valued headers a parser reads; get_all and iteration
 * expose every entry in order for the cookie/challenge layer that needs the
 * full set. There is deliberately no operator[]: it cannot express the
 * replace-vs-append choice that repeated names require, so mutation is the
 * explicit set (replace) or append (add).
 */
class Headers {
public:
	using value_type     = std::pair<std::string, std::string>;
	using container_type = std::vector<value_type>;
	using const_iterator = container_type::const_iterator;

	Headers() = default;

	/** Construct from a brace list; entries keep their order and duplicates. */
	Headers(std::initializer_list<value_type> entries);

	/**
	 * @brief Whether any header has this name.
	 * @param name Header name, matched case-insensitively.
	 * @return True if at least one entry matches.
	 */
	[[nodiscard]] bool contains(std::string_view name) const noexcept;

	/**
	 * @brief The first value for a name, empty if there is none.
	 * @param name Header name, matched case-insensitively.
	 * @return A copy of the first matching value, or an empty string when absent.
	 */
	[[nodiscard]] std::string get(std::string_view name) const;

	/**
	 * @brief The first value for a name.
	 * @param name Header name, matched case-insensitively.
	 * @return A reference to the first matching value.
	 * @throws std::out_of_range if no header has this name.
	 */
	[[nodiscard]] const std::string& at(std::string_view name) const;

	/**
	 * @brief Every value for a name, in order.
	 * @param name Header name, matched case-insensitively.
	 * @return Views of all matching values (e.g. multiple Set-Cookie lines);
	 *         empty if none. The views borrow from this Headers.
	 */
	[[nodiscard]] std::vector<std::string_view> get_all(std::string_view name) const;

	/**
	 * @brief Set a name to a single value, replacing any existing entries for it.
	 * @param name  Header name (its casing is stored as given).
	 * @param value Value to store.
	 */
	void set(std::string_view name, std::string value);

	/**
	 * @brief Add a value without touching existing entries of the same name.
	 * @param name  Header name (its casing is stored as given).
	 * @param value Value to add; a later entry for an already-present name.
	 */
	void append(std::string_view name, std::string value);

	/**
	 * @brief Remove every entry with a name.
	 * @param name Header name, matched case-insensitively.
	 * @return How many entries were removed.
	 */
	size_t erase(std::string_view name);

	/**
	 * @brief Add each entry of another set whose name is not already present.
	 * @param other Defaults to fold in; an existing name here is left untouched,
	 *              so these values lose to the ones already set.
	 */
	void merge_missing(const Headers& other);

	[[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
	[[nodiscard]] size_t size() const noexcept { return entries_.size(); }
	[[nodiscard]] const_iterator begin() const noexcept { return entries_.begin(); }
	[[nodiscard]] const_iterator end() const noexcept { return entries_.end(); }

	/** Equal when the entries match one-for-one in order (names case-insensitive). */
	[[nodiscard]] bool operator==(const Headers& other) const;

private:
	static constexpr size_t npos = static_cast<size_t>(-1);

	/**
	 * @brief Case-insensitive equality of two header names.
	 * @param left  First name.
	 * @param right Second name.
	 * @return True if they are equal ignoring ASCII case.
	 */
	[[nodiscard]] static bool iequal(std::string_view left, std::string_view right) noexcept;

	/**
	 * @brief Index of the first entry with a name.
	 * @param name Header name, matched case-insensitively.
	 * @return The entry index, or @ref npos if no entry matches.
	 */
	[[nodiscard]] size_t find_index(std::string_view name) const noexcept;

	container_type entries_;
};

/**
 * @brief Join headers into "Name: Value" lines for the transport layer.
 * @param headers Headers to serialize, emitted in their stored order.
 * @return List of header lines.
 */
std::list<std::string> to_header_lines(const Headers& headers);

/**
 * @brief Parse a raw response header block into Headers.
 * Inverse of @ref to_header_lines. Accepts a block of CRLF- or LF-delimited
 * "Name: Value" lines (a trailing CR is tolerated). Lines with no colon (a
 * status line, a blank separator, an obsolete fold continuation) are skipped;
 * optional whitespace around the value is trimmed. Duplicate names are kept in
 * order (each Set-Cookie survives; the cookie jar reads them from get_all).
 * @param block Raw header block.
 * @return Parsed headers.
 */
Headers parse_header_block(std::string_view block);

} // namespace aniparse
