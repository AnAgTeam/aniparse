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
 * Compatible catalogs of that same parser may share the format; catalog identity
 * must still be retained when needed to select the restoring root getter. This
 * does not permit another parser to construct or interpret the blob, even when
 * both parsers address the same site. Use a destination URL or external identity
 * for a cross-parser reference, and let the receiving parser create its own getter.
 *
 * @warning **This is a durable format, not a transit token.** A consumer's stored
 *          library holds these blobs for as long as the item is in it — years, and
 *          across upgrades of the parser. So the encoding a parser emits is part of
 *          its public contract: from_serialized() must keep accepting *every form
 *          that parser has ever emitted*. Change the encoding freely, but do not
 *          drop the old one — reading it must keep working, or every stored item
 *          silently loses its binding. Version the payload (a reserved @c params
 *          key) if that ever gets hard to do by inspection.
 *
 * A parser can emit a format version in @c params (for example,
 * @c format_version). Its from_serialized() implementation interprets an absent
 * version as the legacy format, validates known versions, and translates old
 * addressing data into a getter using the current representation. The restored
 * getter's serialize() emits the current format; the consumer must explicitly
 * persist that result to finish the migration. Unknown versions must produce an
 * error rather than be guessed at. A version describes the identity encoding,
 * not the parser release or the fetched metadata revision. Conversion requires
 * a reliable mapping; a version alone cannot recover a renamed source key.
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
