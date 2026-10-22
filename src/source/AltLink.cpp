/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#include "aniparse/source/AltLink.hpp"
#include "aniparse/runtime/RequestContext.hpp"

namespace aniparse {
std::vector<AltLink> resolve_alt_links(std::span<const std::string_view> builtin,
                                       const RequestContext& context) {
	// The context overlays a live catalog override for its parser (if any) onto the
	// built-in list, so the returned list is the one actually fetched from.
	Mirrors mirrors = context.mirrors(builtin);
	std::vector<AltLink> resolved;
	resolved.reserve(mirrors.count());
	for (std::size_t index = 0; index < mirrors.count(); ++index) {
		resolved.push_back(AltLink{ std::string(mirrors.base_url(index)) });
	}
	return resolved;
}

} // namespace aniparse
