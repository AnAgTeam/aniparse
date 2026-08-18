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

SelectorSource::SelectorSource(Overrides overrides)
    : overrides_(std::move(overrides)) {}

SelectorSource::SelectorSource(Overrides shared, ScopedOverrides scoped)
    : overrides_(std::move(shared)), scoped_(std::move(scoped)) {}

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
	return overrides_.empty() && scoped_.empty();
}

bool SelectorSource::has_scope(std::string_view parser_id) const {
	auto scope = scoped_.find(parser_id);
	return scope != scoped_.end() && !scope->second.empty();
}

std::shared_ptr<const SelectorSource> SelectorSource::scoped_to(std::string_view parser_id) const {
	// The flat table is visible to every scope; the parser's scoped entries are
	// merged over it and win on a key collision (the more specific of the two).
	Overrides merged = overrides_;
	if (auto scope = scoped_.find(parser_id); scope != scoped_.end()) {
		for (const auto& [key, css] : scope->second) {
			merged.insert_or_assign(key, css);
		}
	}
	return std::make_shared<const SelectorSource>(std::move(merged));
}

} // namespace aniparse::html
