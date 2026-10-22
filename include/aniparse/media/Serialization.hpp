/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <functional>
#include <map>
#include <string>

namespace aniparse {
/**
 * @brief A getter's identity, in a form that survives the process: what
 * serialize() emits and from_serialized() takes back to rebuild the same getter.
 *
 * Encodes identity only — never the fetched Info, which is a cache and not part
 * of what addresses the item. The shape is deliberately not a bare id: a source
 * may need several values to address one item (a slug plus the section it lives
 * in), which a single string cannot carry.
 *
 * **Owned by the parser that emitted it.** The consumer moves it, stores it, and
 * hands it back verbatim; it never reads, writes or invents the fields, and it
 * never mixes its own values in. Same discipline as Tag::ref. Across parsers the
 * blob means nothing: it is addressed only together with the parser that produced
 * it, so a consumer persisting one persists the parser's identifier beside it.
 *
 * @warning **This is a durable format, not a transit token.** A consumer's stored
 *          library holds these blobs for as long as the item is in it — years, and
 *          across upgrades of the parser. So the encoding a parser emits is part of
 *          its public contract: from_serialized() must keep accepting *every form
 *          that parser has ever emitted*. Change the encoding freely, but do not
 *          drop the old one — reading it must keep working, or every stored item
 *          silently loses its binding. Version the payload (a reserved @c params
 *          key) if that ever gets hard to do by inspection.
 */
struct SerializedGetterData {
	/// The item's addressing handle: whatever the parser identifies by — an id, a
	/// slug, a path. Opaque to the consumer despite the name; not necessarily a URL.
	std::string url;
	/// Any further values the parser needs to address the item, when @ref url alone
	/// is not enough. Parser-owned keys; empty for most sources.
	std::map<std::string, std::string, std::less<>> params;
};
} // namespace aniparse
