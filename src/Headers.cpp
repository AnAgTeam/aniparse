/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Headers.hpp"

#include <stdexcept>

namespace aniparse {

Headers::Headers(std::initializer_list<value_type> entries) {
	for (const value_type& entry : entries) {
		append(entry.first, entry.second);
	}
}

bool Headers::contains(std::string_view name) const noexcept {
	return find_index(name) != npos;
}

std::string Headers::get(std::string_view name) const {
	size_t index = find_index(name);
	return index == npos ? std::string{} : entries_[index].second;
}

const std::string& Headers::at(std::string_view name) const {
	size_t index = find_index(name);
	if (index == npos) {
		throw std::out_of_range("Headers::at: no header named " + std::string(name));
	}
	return entries_[index].second;
}

std::vector<std::string_view> Headers::get_all(std::string_view name) const {
	std::vector<std::string_view> values;
	for (const value_type& entry : entries_) {
		if (iequal(entry.first, name)) {
			values.push_back(entry.second);
		}
	}
	return values;
}

void Headers::set(std::string_view name, std::string value) {
	erase(name);
	entries_.emplace_back(std::string(name), std::move(value));
}

void Headers::append(std::string_view name, std::string value) {
	entries_.emplace_back(std::string(name), std::move(value));
}

size_t Headers::erase(std::string_view name) {
	size_t before = entries_.size();
	std::erase_if(entries_, [&](const value_type& entry) { return iequal(entry.first, name); });
	return before - entries_.size();
}

void Headers::merge_missing(const Headers& other) {
	size_t original = entries_.size();
	for (const value_type& entry : other.entries_) {
		bool present = false;
		for (size_t i = 0; i < original; ++i) {
			if (iequal(entries_[i].first, entry.first)) {
				present = true;
				break;
			}
		}
		if (!present) {
			append(entry.first, entry.second);
		}
	}
}

bool Headers::operator==(const Headers& other) const {
	if (entries_.size() != other.entries_.size()) {
		return false;
	}
	for (size_t i = 0; i < entries_.size(); ++i) {
		if (!iequal(entries_[i].first, other.entries_[i].first) ||
		    entries_[i].second != other.entries_[i].second) {
			return false;
		}
	}
	return true;
}

bool Headers::iequal(std::string_view left, std::string_view right) noexcept {
	if (left.size() != right.size()) {
		return false;
	}
	for (size_t i = 0; i < left.size(); ++i) {
		if (CaseInsensitiveLess::to_lower(left[i]) != CaseInsensitiveLess::to_lower(right[i])) {
			return false;
		}
	}
	return true;
}

size_t Headers::find_index(std::string_view name) const noexcept {
	for (size_t i = 0; i < entries_.size(); ++i) {
		if (iequal(entries_[i].first, name)) {
			return i;
		}
	}
	return npos;
}

std::list<std::string> to_header_lines(const Headers& headers) {
	std::list<std::string> lines;
	for (const auto& [name, value] : headers) {
		lines.push_back(name + ": " + value);
	}
	return lines;
}

Headers parse_header_block(std::string_view block) {
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

		headers.append(std::string(name), std::string(value));
	}
	return headers;
}

} // namespace aniparse
