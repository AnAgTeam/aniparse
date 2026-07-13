/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Ids.hpp"
#include "aniparse/types/Model.hpp"
#include "aniparse/types/Text.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

/*
 * The manga DATA model, split out of Manga.hpp so it can be included without the
 * getter interfaces — and therefore without coroutines, expected, or the client.
 * Manga.hpp includes this and adds the getters, so nothing else changes.
 *
 * Kept transport-free on purpose: this is the half a consumer binds to (the Swift
 * bridge imports it directly), and a header that reaches NetworkRequestTask cannot
 * be imported by Swift's clang importer at all.
 */
namespace aniparse {

struct MangaGetter;

using MangaID            = int;
using MangaTranslationID = int;

struct MangaGetter;

enum class DefaultMangaType {
	Manga,
	Dojinshi,
	Manhwa,
	Manhua,
	Other
};

struct MangaType {
	MangaType(DefaultMangaType type);

	std::string name;
};

inline constexpr MangaID invalid_manga_id                 = MangaID{ 0 };
inline constexpr MangaTranslationID any_manga_translation = -1;

struct MangaInfo {
	MangaID id = invalid_manga_id;

	std::string title;
	std::optional<std::string> original_title;
	AttributedText description;

	std::chrono::system_clock::time_point update_time  = unknown_time;
	std::chrono::system_clock::time_point release_time = unknown_time;
	AiredStatus status;

	/// Opaque change marker for the whole manga, filled from the cheapest
	/// signal the source exposes (an ETag, an updated-at value, an explicit
	/// version, or a composite). Compared only for equality: a changed value
	/// means the source reports the content as a different revision. Equality
	/// is a cheap "probably unchanged" hint, not a content-integrity guarantee
	/// (it does not catch a single re-uploaded chapter or mid-list id drift).
	/// Empty = the source exposes no such signal.
	std::string revision;

	RelatedUser author;
	RelatedUser artist;
	std::optional<Series> series;

	std::vector<Image> previews;
	std::vector<Tag> tags;

	std::optional<Rating> rating;
	std::optional<ViewStats> views;
	std::optional<UserList> user_lists;
	AgeRestriction age_restriction = 0;

	std::optional<RelatedUser> uploader;

	std::optional<long> total_chapters;

	bool is_hentai = false;
};

struct MangaTranslationInfo {
	MangaTranslationID id;
	std::string language;
	RelatedUser translator;
};

/**
 * @brief Identity of one chapter: everything chapter_pages() needs and
 * nothing else, so the call stays cheap (typically zero heap: two longs
 * plus an empty/SSO string).
 * Obtained via MangaChapterInfo::ref() and round-tripped unchanged; for
 * simple numerically-addressed sources it can also be constructed
 * directly, e.g. MangaChapterRef{ .chapter = 12 }.
 */
struct MangaChapterRef {
	long volume  = 0;
	long chapter = 0;
	/// Opaque handle from the getter that produced the info.
	/// Empty = the getter identifies the chapter by the numeric fields
	std::string id;
};

struct MangaChapterInfo {
	/// Numeric hints for grouping/ordering in UI; not the chapter's identity.
	/// Sources with fractional or unnumbered chapters ("7.5", "Extra") cannot
	/// express them losslessly here — that is what number and id are for
	long volume  = 0;
	long chapter = 0;
	/// Chapter number exactly as the source spells it: "7.5", "Extra".
	/// Empty = render from volume/chapter
	std::string number;
	/// Opaque chapter handle, understood only by the getter that produced
	/// this info; round-trips into chapter_pages() via ref().
	std::string id;
	std::string name;
	std::string description;
	std::vector<Image> previews;
	std::chrono::system_clock::time_point update_time  = unknown_time;
	std::chrono::system_clock::time_point release_time = unknown_time;

	/// Identity for the chapter_pages() round-trip
	[[nodiscard]] MangaChapterRef ref() const { return { volume, chapter, id }; }
};

struct MangaPage {
	Image image;
};

} // namespace aniparse
