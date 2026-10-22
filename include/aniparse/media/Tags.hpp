/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/media/Ids.hpp"
#include <string>
#include <string_view>

namespace aniparse {

/**
 * @brief One label an item carries: a genre, a theme, a character, an artist —
 * whatever axis the source tags by, flattened into a single list.
 *
 * The list is the source's own vocabulary; the library does not normalize it, so
 * two parsers may spell the same concept differently. What makes a tag more than a
 * string is @ref Tag::ref — where the source can search by tags, the ref is the token that
 * searches for this tag, so a tag read off an item can be fed straight back into a
 * query.
 */
struct Tag {
	/// Source-local numeric handle for the tag, when the producing parser carries
	/// one; 0 = none, which is the usual case. Not the tag's identity for search —
	/// @ref Tag::ref is. Never assume it is comparable across sources.
	TagID id{0};
	/// Human-readable label, in whatever wording and language the source uses.
	/// Display-only: two sources may name the same concept differently, and the
	/// same tag may read differently on the same source over time.
	std::string name;
	/// The categorical axis this tag belongs to: a @ref aniparse::search_keys value
	/// (@ref search_keys::tag, @ref search_keys::genre, @ref search_keys::character,
	/// @ref search_keys::type, @ref search_keys::language, ...). It names the filter
	/// GROUP; @ref ref is the token WITHIN it, so the pair (axis, ref) fully addresses
	/// this tag in a query — search() routes ref into the filter keyed by axis.
	/// Empty = @ref search_keys::tag, the default axis, so a tag that predates this
	/// field (or a source with a single, unnamed tag axis) behaves exactly as before.
	/// Where the source has no per-axis filter at all, ref carries the whole token and
	/// axis is a display-only grouping hint. A consumer may section its tag display by
	/// this value, and MUST degrade to a flat list when it is empty.
	std::string axis;
	/// Opaque parser-owned handle for this tag. NOT an HTTP Referer and not for
	/// the consumer to interpret — it is moved, never parsed. It round-trips to
	/// build follow-up requests; and where the parser supports searching by tag,
	/// it is the tag's search token: the same value advertised as the option's
	/// SelectionValue token (search_keys::tag) and accepted back in search(),
	/// so a Tag from MangaInfo drops straight into a query.
	/// Prefer the source's stable identity (a slug/id) over a request path or
	/// URL, so a route or mirror change does not invalidate a stored handle; the
	/// parser rebuilds the request from it.
	/// Empty = this tag is not addressable on the source: it is a label to show,
	/// but nothing can be requested with it (a source that displays an axis it
	/// cannot filter by leaves the ref empty). Only a non-empty ref may be used as
	/// a search token.
	std::string ref;
};

/**
 * The conventional @ref aniparse::Series::name / @ref aniparse::Series::ref for a work that belongs to
 * no franchise — an original creation rather than a derivative of something else.
 * Sources that tag by franchise commonly use this exact token for that case, so
 * compare against this constant instead of treating it as a real series.
 */
inline constexpr std::string_view series_original = "original";

/**
 * @brief The franchise or parent work an item belongs to (its "copyright" on a
 * tag-based source, its parent series elsewhere) — the axis a consumer groups
 * derivative items by.
 */
struct Series {
	/// Display name of the series. @see series_original for the no-franchise case.
	std::string name;
	/// Opaque parser-owned reference; @see Tag::ref. Where the source can search by
	/// series it is that search token (@see search_keys::series); empty = the series
	/// is a label only and cannot be searched for here.
	std::string ref;
};

} // namespace aniparse
