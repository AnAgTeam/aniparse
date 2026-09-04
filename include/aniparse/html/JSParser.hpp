/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <optional>
#include <boost/json.hpp>

/**
 * @file
 * Getting at the data a page keeps in an inline `<script>` rather than in its
 * markup: locate a JavaScript object/array literal by variable name and hand it
 * back — as a view, or parsed into JSON. The scanner is brace-balancing, not a
 * JS engine; it skips strings, template literals and comments so punctuation
 * inside them cannot end the literal early. The parsing half of the JSON layer;
 * the walking half is aniparse/json/Json.hpp.
 */

namespace aniparse::html {

/**
 * @brief Find the literal value assigned to a JavaScript variable.
 * Locates a top level assignment of an object or array literal by name:
 * @code{.js}
 *   var <name> = { ... };
 *   <name> = [ ... ];
 *   "<name>": [ ... ]
 * @endcode
 * and returns the balanced literal with its braces/brackets included, ready
 * to be handed to a JSON parser such as boost::json::parse. While matching
 * braces it skips string and template literals and comments, so punctuation
 * inside them does not break the scan.
 * @note The returned view points into @p text, so it stays valid only as
 *       long as @p text is alive. Regex literals inside the value are not
 *       recognised (rare in data blobs).
 * @param variable_name The name of the variable to search for
 * @param text The text to search in
 * @return View of the object/array literal, or an empty view if not found
 */
extern std::string_view find_json_var(std::string_view variable_name, const std::string& text);
extern std::string_view find_json_var(std::string_view variable_name, std::string&& text) = delete;

/**
 * @brief Find a JavaScript object/array literal by name and parse it as JSON.
 * Convenience wrapper over @ref aniparse::html::find_json_var : locates the literal, then parses
 * it with boost::json (comments and trailing commas are tolerated). Unlike
 * find_json_var it returns an owning value, so @p text may be a temporary and
 * no dangling can occur.
 * @note Tolerates common JS-isms: single quoted strings (normalized to double
 *       quotes), comments and trailing commas. JavaScript that is still not
 *       valid JSON after that yields std::nullopt; use @ref aniparse::html::find_json_var and
 *       handle such data yourself.
 * @param variable_name The name of the variable to search for
 * @param text The text to search in
 * @return The parsed value, or std::nullopt if not found or not valid JSON
 */
extern std::optional<boost::json::value> parse_json_var(std::string_view variable_name, std::string_view text);

/**
 * @brief Find an object/array literal supplied at a function-call argument position.
 *
 * Locates a call to @p callee, skips arguments before @p argument_index while
 * respecting nested expressions, strings, template literals, and comments, then
 * returns the selected argument when it starts with @c { or @c [. The returned
 * view borrows from @p text.
 * @param callee Complete function name, for example @c "Player.init".
 * @param argument_index Zero-based argument position.
 * @param text JavaScript source to search.
 * @return The balanced JSON-like literal, or an empty view.
 */
extern std::string_view find_json_call_argument(std::string_view callee, std::size_t argument_index,
                                                std::string_view text);

/**
 * @brief Find and parse a JSON-like literal supplied to a function call.
 *
 * Equivalent to @c find_json_call_argument followed by the same JS-to-JSON
 * normalisation used by @c parse_json_var.
 */
extern std::optional<boost::json::value> parse_json_call_argument(
    std::string_view callee, std::size_t argument_index, std::string_view text);

} // namespace aniparse::html
