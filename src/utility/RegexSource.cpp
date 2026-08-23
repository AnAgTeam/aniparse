/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/utility/RegexSource.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace aniparse {

namespace {

// A compiled regex does not expose its group names, so both the derivation and
// the declaration check are textual. The named-group contract of a pattern is
// DERIVED from its built-in literal: every group the consumer can read back by
// name is spelled in the literal as (?<name>, (?'name', or (?P<name>.
//
// The scanner is escape- and char-class-aware so that literal text spelling the
// same bytes — \(\?< inside a pattern that matches a verbatim "(?<", or the
// tokens inside a [...] class — is not misread as a group declaration.
std::vector<std::string_view> named_groups_of(std::string_view pattern) {
	std::vector<std::string_view> groups;
	bool in_class = false;
	for (size_t i = 0; i < pattern.size();) {
		const char c = pattern[i];
		if (c == '\\') {
			i += 2; // escaped char: never a token start
			continue;
		}
		if (in_class) {
			if (c == ']') in_class = false;
			++i;
			continue;
		}
		if (c == '[') {
			in_class = true;
			++i;
			continue;
		}
		if (c != '(' || i + 3 >= pattern.size() || pattern[i + 1] != '?') {
			++i;
			continue;
		}
		// After "(?: lookbehinds (?<= / (?<! and comments (?# are not groups;
		// the named forms are (?<name>, (?'name', (?P<name>.
		size_t name_begin = 0;
		char close = '>';
		if (pattern[i + 2] == '<' && pattern[i + 3] != '=' && pattern[i + 3] != '!') {
			name_begin = i + 3;
		} else if (pattern[i + 2] == '\'') {
			name_begin = i + 3;
			close = '\'';
		} else if (pattern[i + 2] == 'P' && i + 4 < pattern.size() && pattern[i + 3] == '<') {
			name_begin = i + 4;
		} else {
			++i;
			continue;
		}
		const size_t name_end = pattern.find(close, name_begin);
		if (name_end == std::string_view::npos || name_end == name_begin) {
			++i;
			continue;
		}
		const std::string_view name = pattern.substr(name_begin, name_end - name_begin);
		if (std::ranges::find(groups, name) == groups.end()) {
			groups.push_back(name);
		}
		i = name_end + 1;
	}
	return groups;
}

// The override check is textual: the pattern must spell the group as (?<name>,
// (?'name', or (?P<name>. An override missing a contracted group would silently
// feed its consumer an empty capture, so it degrades to the built-in instead.
bool declares_group(std::string_view pattern, std::string_view name) {
	const std::string angled = "(?<" + std::string(name);
	const std::string quoted = "(?'" + std::string(name);
	const std::string pythonic = "(?P<" + std::string(name);
	return pattern.find(angled) != std::string_view::npos ||
	       pattern.find(quoted) != std::string_view::npos ||
	       pattern.find(pythonic) != std::string_view::npos;
}

bool declares_all(std::string_view pattern, std::span<const std::string_view> groups) {
	return std::ranges::all_of(groups, [pattern](std::string_view group) {
		return declares_group(pattern, group);
	});
}

} // namespace

const RegexSource& RegexSource::empty() noexcept {
	static const RegexSource instance;
	return instance;
}

RegexSource::RegexSource(Overrides overrides)
    : overrides_(std::move(overrides)) {}

RegexSource::RegexSource(ScopedOverrides scoped)
    : scoped_(std::move(scoped)) {}

regex RegexSource::compile(std::string_view key, std::string_view fallback) const {
	// The contract is derived from the built-in literal: the groups it declares
	// are exactly the ones its consumer may read back by name, so an override
	// must declare the same set to be accepted.
	const std::vector<std::string_view> required_groups = named_groups_of(fallback);
	if (auto entry = overrides_.find(key); entry != overrides_.end()) {
		// Override off the net: it must declare every contracted group and
		// compile; either failure drops it in favour of the built-in default.
		if (declares_all(entry->second, required_groups)) {
			try {
				return regex(entry->second);
			} catch (const boost::regex_error&) {
				// fall through to the built-in default
			}
		}
	}
	// Built-in literal: strict. A broken default is a programming error a test
	// must catch loudly, not a runtime condition.
	return regex(std::string(fallback));
}

std::string_view RegexSource::get(std::string_view key, std::string_view fallback) const {
	auto entry = overrides_.find(key);
	return entry != overrides_.end() ? std::string_view(entry->second) : fallback;
}

bool RegexSource::empty_source() const noexcept {
	return overrides_.empty() && scoped_.empty();
}

bool RegexSource::has_scope(std::string_view owner) const {
	auto scope = scoped_.find(owner);
	return scope != scoped_.end() && !scope->second.empty();
}

std::shared_ptr<const RegexSource> RegexSource::scoped_to(std::string_view owner) const {
	Overrides view;
	if (auto scope = scoped_.find(owner); scope != scoped_.end()) {
		view = scope->second;
	}
	return std::make_shared<const RegexSource>(std::move(view));
}

} // namespace aniparse
