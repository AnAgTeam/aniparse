/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/JSParser.hpp"
#include "aniparse/utility/Regex.hpp"

namespace aniparse::html {

	// TODO:
	std::string_view find_json_var_object(std::string_view variable_name, const std::string& text) {
		static const regex var_object_re(R"(\{(?:[^{}]+|(?0))*\})");
		
		return "";
	}
}