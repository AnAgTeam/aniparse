/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include <string_view>
#include <boost/json.hpp>

 /*
	 In future maybe add dedicated parser for JavaScript.
	 Right now it isn't something important, because
	 in most cases can use regex.
 */

namespace aniparse::html {

	/**
	 * @note The method requires boost::regex for windows with MSVC.
	 *       MSVC cannot compile regex.
	 * @brief Find JavaScript variable declaration with JSON object.
	 * Object variable declaration looks like:
	 *   var <name> = { ... };
	 * @param variable_name The "name" of the variable to search
	 * @param text The text in which the variable be searched
	 * @return String view of the JSON
	 */
	extern std::string_view find_json_var_object(std::string_view variable_name, const std::string& text);
	extern std::string_view find_json_var_object(std::string_view variable_name, std::string&& text) = delete;
}