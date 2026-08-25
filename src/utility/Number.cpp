/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/utility/Number.hpp"

namespace aniparse::number {
std::optional<double> parse_decimal_prefix(std::string_view text) noexcept {
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' || text.front() == '\r')) {
		text.remove_prefix(1);
	}

	double whole = 0.0;
	double fraction = 0.0;
	double scale = 1.0;
	bool after_decimal = false;
	bool has_digit = false;
	for (const char character : text) {
		if (!after_decimal && (character == '.' || character == ',')) {
			after_decimal = true;
			continue;
		}
		if (character < '0' || character > '9') {
			break;
		}
		has_digit = true;
		if (after_decimal) {
			scale *= 10.0;
			fraction = fraction * 10.0 + (character - '0');
		} else {
			whole = whole * 10.0 + (character - '0');
		}
	}
	if (!has_digit) {
		return std::nullopt;
	}
	return whole + fraction / scale;
}
} // namespace aniparse::number
