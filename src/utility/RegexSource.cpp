/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/utility/RegexSource.hpp"

#include <algorithm>
#include <stdexcept>

namespace aniparse {

namespace {

// A compiled regex does not expose its group names, so the declaration check is
// textual: the pattern must spell the group as (?<name> or (?'name'.
bool declares_group(std::string_view pattern, std::string_view name) {
	const std::string angled = "(?<" + std::string(name);
	const std::string quoted = "(?'" + std::string(name);
	return pattern.find(angled) != std::string_view::npos ||
	       pattern.find(quoted) != std::string_view::npos;
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

regex RegexSource::compile(std::string_view key, std::string_view fallback,
                           std::span<const std::string_view> required_groups) const {
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
	// Built-in literal: strict. A broken default or a missing group declaration
	// is a programming error a test must catch loudly, not a runtime fallback.
	if (!declares_all(fallback, required_groups)) {
		throw std::logic_error("built-in regex pattern '" + std::string(key) +
		                       "' does not declare a required named group");
	}
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
