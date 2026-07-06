/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/html/SelectorSource.hpp"

namespace aniparse::html {

const SelectorSource& SelectorSource::empty() noexcept {
	static const SelectorSource instance;
	return instance;
}

SelectorSource::SelectorSource(std::map<std::string, std::string, std::less<>> overrides)
    : overrides_(std::move(overrides)) {}

CompiledSelector SelectorSource::compile(SelectorCompiler& compiler,
                                         std::string_view key,
                                         std::string_view fallback) const {
	if (auto entry = overrides_.find(key); entry != overrides_.end()) {
		// Override off the net: parse non-throwing so a bad hotfix cannot brick
		// set construction — on failure fall through to the built-in default.
		if (auto compiled = compiler.try_compile(entry->second)) {
			return std::move(*compiled);
		}
	}
	// Built-in literal: compiled strictly. A broken default is a programming error
	// a test must catch loudly, not a runtime fallback.
	return compiler.compile(fallback);
}

std::string_view SelectorSource::get(std::string_view key, std::string_view fallback) const {
	auto entry = overrides_.find(key);
	return entry != overrides_.end() ? std::string_view(entry->second) : fallback;
}

bool SelectorSource::empty_source() const noexcept {
	return overrides_.empty();
}

} // namespace aniparse::html
