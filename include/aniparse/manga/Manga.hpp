/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Common.hpp"
#include "aniparse/ParsedUrl.hpp"

#include <coro/expected.hpp>

namespace aniparse {

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

/// A selectable mirror (base URL) for a source. Selected by index via
/// RequestorContext::alt_link(). Kept as a struct so per-mirror metadata can be
/// added later without changing the collection type.
struct AltLink {
	std::string url;
};

struct MangaGetterCompatibilities {
	std::vector<AltLink> alt_links;
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};

struct MangaGetterRootCompatibilities {
	SupportedSorts supported_sorts;
	CompatibilitiesFlags compatibilities = compatibilities_flags::default_flags;
};

struct SearchCompatibilities {
	SearchItems supported_filters;
	SupportedSorts supported_sorts;
	CompatibilitiesFlags compatibilities = compatibilities_flags::default_flags;
};

/**
 * @brief Interface for getting one specific manga information.
 */
struct MangaGetter {
	virtual ~MangaGetter() = default;

	virtual MangaGetterCompatibilities compatibilities() const noexcept = 0;

	virtual NetworkRequestTask<MangaInfo> preview_info(RequestorContext context);

	virtual NetworkRequestTask<MangaInfo> info(RequestorContext context) = 0;

	virtual NetworkRequestTask<PageResults<MangaTranslationInfo>> translation_info(
	    RequestorContext context,
	    GetFilters filters);

	virtual NetworkRequestTask<PageResults<MangaChapterInfo>> chapters_info(
	    RequestorContext context,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation = std::nullopt);

	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> related(
	    RequestorContext context,
	    GetFilters filters);

	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> similar(
	    RequestorContext context,
	    GetFilters filters);

	/**
	 * Get the pages of one chapter.
	 * The chapter is identified by round-trip: pass MangaChapterInfo::ref()
	 * of an item from chapters_info() (its id may carry a source-specific
	 * handle). @see MangaChapterRef
	 */
	virtual NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
	    RequestorContext context,
	    MangaChapterRef chapter,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation = std::nullopt) = 0;

	virtual void reset() noexcept;

	virtual NetworkRequestTask<SerializedGetterData> serialize() = 0;
};

/**
 * @brief Root interface for manga. Used to get pages or general information
 */
struct MangaRootGetter {
	virtual ~MangaRootGetter() = default;

	virtual SearchCompatibilities search_support() const noexcept;
	virtual MangaGetterRootCompatibilities latest_support() const noexcept;

	/**
	 * @brief Check query filters and the requested sort against this getter's
	 * own search_support(). Keeps the declaration the single source of truth:
	 * call at the start of search() and report InvalidArguments (see
	 * describe_search_query_errors) instead of silently ignoring unsupported
	 * filters.
	 * @return Empty if the query is valid; otherwise all violations found
	 */
	[[nodiscard]] std::vector<SearchQueryError> validate_query(
	    const SearchRequestQuery& query,
	    const GetFilters& filters) const;

	/**
	 * @brief Check the requested sort against this getter's own
	 * latest_support(). Same contract as validate_query, for latest().
	 * @return Empty if the filters are valid; otherwise the violation
	 */
	[[nodiscard]] std::vector<SearchQueryError> validate_latest_filters(const GetFilters& filters) const;

	/**
	 * @todo !

	 * @note By default passed client is forwarded.
	 */
	virtual std::shared_ptr<ParserConfig> default_config_from(std::shared_ptr<const ParserConfig> base_config) const;

	/**
	 * @todo
	 * Search mangas with query and/or filters (advanced query may come as filters)
	 * By default throws NotImplementedError
	 * @param context Client to perform HTTP requests
	 * @param query Query string, plain text
	 * @param filters Filters to apply to results (e.g. sort ...)
	 * @throw NotImplementedError If the method isn't implemented by the parser
	 * @return ...
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters);

	/**
	 * @todo
	 * Get latest parser source released mangas
	 * By default throws NotImplementedError
	 * @param context Client to perform HTTP requests
	 * @param filters Filters to apply to results (e.g. sort ...)
	 * @throw NotImplementedError If the method isn't implemented by the parser
	 * @return ...
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters);

	/**
	 * @see MangaGetter
	 * Parse the url and return corresponding getter
	 * By default throws NotImplementedError
	 * @param context Client to perform HTTP requests
	 * @param url The url to parse
	 * @throw NotImplementedError If the method isn't implemented by the parser
	 * @return Task to get MangaGetter
	 */
	virtual NetworkRequestTask<std::unique_ptr<MangaGetter>> parse_url(
	    RequestorContext context,
	    ParsedUrl url);

	/**
	 * @brief Getter for serialized data from one of serialize() methods
	 */
	virtual NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(SerializedGetterData data) = 0;
};
} // namespace aniparse