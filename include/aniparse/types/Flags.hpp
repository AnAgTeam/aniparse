/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/FlagsBitfield.hpp"

/**
 * @file
 * The capability vocabulary: one flag set shared by every "what can this thing
 * do?" descriptor in the library — the parser-level @ref aniparse::ParserCompatibilities,
 * and the per-getter tables (SearchCompatibilities, MangaGetterCompatibilities,
 * …). The bits live in one namespace so that a capability means the same thing
 * wherever it is advertised; which descriptor a given bit is meaningful on is
 * stated on the bit itself.
 *
 * Flags are a declaration, never a promise a caller may skip checking: they let a
 * consumer decide what to offer (a comment tab, a track picker, an adult-content
 * gate) before spending a request, but a source may still answer an unsupported
 * call with a RequestError.
 */

namespace aniparse {
/**
 * @brief A set of capability bits, drawn from the compatibilities_flags namespace.
 * A strongly typed 64-bit flag word: combine bits with @c |, query a mask with
 * @c has(). The tag makes it a distinct type from other flag words, so a
 * capability bit cannot be passed where an unrelated flag set is expected.
 */
using CompatibilitiesFlags = FlagsBitfield<64, struct CompatibilitiesFlagsTag>;
} // namespace aniparse

/**
 * @brief The capability bits a parser or a getter can advertise.
 * @see CompatibilitiesFlags, ParserCompatibilities, SearchCompatibilities
 */
namespace aniparse::compatibilities_flags {
/**
 * Reserved. No facility of the library reads this bit today; it is kept for bit
 * stability (the bit positions are part of the advertised value) and carries no
 * defined contract for a consumer.
 */
inline constexpr auto supports_inplace_get  = CompatibilitiesFlags::make_bit(0);
/// The source offers a browsable image library. Advertised on
/// ParserCompatibilities by a parser that also hands out an ImagesGetter.
inline constexpr auto supports_images_store = CompatibilitiesFlags::make_bit(1);
/// The source offers a browsable anime library. Advertised on
/// ParserCompatibilities by a parser that also hands out an AnimeRootGetter.
inline constexpr auto supports_anime_store  = CompatibilitiesFlags::make_bit(2);
/// The source offers a browsable manga library. Advertised on
/// ParserCompatibilities by a parser that also hands out a MangaRootGetter.
inline constexpr auto supports_manga_store  = CompatibilitiesFlags::make_bit(3);
/// The source hosts the reading path itself: its MangaGetter returns real
/// chapter_pages(), not only metadata. A metadata-only catalogue (an aggregator
/// that lists titles but hosts no pages) advertises supports_manga_store WITHOUT
/// this bit. A consumer uses it to tell a reader from a catalogue and to gate the
/// reading UI before a request. @see MangaGetter::chapter_pages
inline constexpr auto supports_reading      = CompatibilitiesFlags::make_bit(16);
/// The source hosts the watching path itself: its AnimeGetter returns playable
/// episode_sources(), not only metadata. A metadata-only anime catalogue
/// advertises supports_anime_store WITHOUT this bit. A consumer uses it to
/// distinguish a video source from a catalogue and to gate the player UI before
/// a request. @see AnimeGetter::episode_sources
inline constexpr auto supports_watching     = CompatibilitiesFlags::make_bit(17);
/// The source offers a browsable video library beyond the anime catalog.
inline constexpr auto supports_video_store  = CompatibilitiesFlags::make_bit(4);
/**
 * Reserved for a source whose library does not fit the domain stores above. No
 * facility of the library reads this bit today.
 */
inline constexpr auto using_custom_store    = CompatibilitiesFlags::make_bit(5);
/**
 * The source hosts adult content. Advertised on ParserCompatibilities so a
 * consumer can gate or hide the source before any request is made; it says
 * nothing about an individual item.
 */
inline constexpr auto adult_source          = CompatibilitiesFlags::make_bit(6);

/// The source's image library can be searched, not only browsed.
inline constexpr auto supports_images_search = CompatibilitiesFlags::make_bit(7);
/// The source accepts new user accounts, so a consumer may offer a sign-up path.
/// Registration itself is out of scope for the library — only login exists
/// (@ref Parser::authenticate_context).
inline constexpr auto supports_registration  = CompatibilitiesFlags::make_bit(8);
/// The source lets an authenticated user rate or vote on items. Declaration only:
/// no getter exposes voting yet.
inline constexpr auto supports_voting        = CompatibilitiesFlags::make_bit(9);
/// The source carries user comments on its items, so the domain getters' @c
/// comments() answer instead of reporting NotImplemented.
inline constexpr auto supports_commenting    = CompatibilitiesFlags::make_bit(10);
/// The source keeps server-side user lists (a reading/watching list bound to the
/// account). Declaration only: no getter exposes list editing yet.
inline constexpr auto supports_online_lists  = CompatibilitiesFlags::make_bit(11);

/// The source can suggest search tokens for a partial input (autocomplete). A
/// getter advertises it in SearchCompatibilities::compatibilities. @see suggest
inline constexpr auto supports_suggestions   = CompatibilitiesFlags::make_bit(14);

/// The anime source exposes a (team x player) track axis to browse before
/// episodes: pick a track, then its episode set. An AnimeGetter advertises it in
/// its compatibilities() so a consumer shows a track picker without a
/// speculative request; episode-first sources leave it unset. @see tracks
inline constexpr auto supports_tracks        = CompatibilitiesFlags::make_bit(15);

/**
 * All the Paginator<T>::next items will be unique over time if true.
 * Otherwise, the items can duplicate. For example, when you get 10
 * from next, then 3 new items added at the start and you start getting
 * duplicates of 8-10 from previous parsing and only 7 new
 */
inline constexpr auto supports_pagination_uniqueness = CompatibilitiesFlags::make_bit(12);

/**
 * A sentinel bit that names no capability: it is never advertised by a source and
 * never satisfied by a support check, so it can stand for "a feature this
 * descriptor cannot express".
 */
inline constexpr auto unsupported_feature = CompatibilitiesFlags::make_bit(13);

/// The empty capability set — advertise nothing. The initial value of every
/// CompatibilitiesFlags field, so a descriptor lists only what it opts into.
inline constexpr CompatibilitiesFlags default_flags;
} // namespace aniparse::compatibilities_flags

namespace aniparse {
/// A parser's declared capabilities, returned by Parser::compatibilities(). The
/// parser-level (cross-domain) capability descriptor; distinct from the per-getter
/// *GetterCompatibilities structs.
struct ParserCompatibilities {
	/// Everything the source declares at parser level: which libraries it offers
	/// (the @c *_store bits), whether it is adult, which account features exist.
	/// Empty by default — a parser lists only what it opts into.
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};
} // namespace aniparse
