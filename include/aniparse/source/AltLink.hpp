/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aniparse {

class RequestContext;

/// A selectable mirror (base URL) for a source. Selected by index via
/// RequestContext::alt_link(). Kept as a struct so per-mirror metadata can be
/// added later without changing the collection type.
struct AltLink {
	/// Base URL of this mirror — the host requests are actually sent to when the
	/// mirror is selected. What a mirror picker shows and selects by.
	std::string url;
};

/**
 * @brief The mirror choices to show the user for a @p builtin mirror list, with
 * any live catalog override applied.
 *
 * Equals @p builtin (as AltLinks) when no catalog overrides the context's parser;
 * otherwise the overridden set — so a picker reflects the mirrors actually fetched
 * from, not a stale built-in list. Generic over any source: the override is keyed
 * by the config's parser id carried in @p context. @see ParserProfile::mirror_choices.
 * @param builtin The source's built-in base URLs (its single source of truth).
 * @param context Context carrying the current mirror snapshot and parser id.
 * @return The resolved mirror descriptors, in selection order.
 */
[[nodiscard]] std::vector<AltLink> resolve_alt_links(std::span<const std::string_view> builtin,
                                                     const RequestContext& context);

} // namespace aniparse
