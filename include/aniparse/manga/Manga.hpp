/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Common.hpp"
#include "aniparse/ParsedUrl.hpp"

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

/// A selectable mirror (base URL) for a source. Selected by index via
/// RequestorContext::alt_link(). Kept as a struct so per-mirror metadata can be
/// added later without changing the collection type.
struct AltLink {
	std::string url;
};

/**
 * @brief The mirror choices to show the user for a @p builtin mirror list, with
 * any live catalog override applied.
 *
 * Equals @p builtin (as AltLinks) when no catalog overrides the context's parser;
 * otherwise the overridden set — so a picker reflects the mirrors actually fetched
 * from, not a stale built-in list. Generic over any source: the override is keyed
 * by the config's parser id carried in @p context. @see Parser::mirror_choices.
 * @param builtin The source's built-in base URLs (its single source of truth).
 * @param context Context carrying the current mirror snapshot and parser id.
 * @return The resolved mirror descriptors, in selection order.
 */
[[nodiscard]] std::vector<AltLink> resolve_alt_links(std::span<const std::string_view> builtin,
                                                     const RequestorContext& context);

struct MangaGetterCompatibilities {
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};

struct MangaGetterRootCompatibilities {
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

	/**
	 * @brief User comments on the manga, paginated via @p filters.
	 * Available when the parser advertises supports_commenting; by default reports
	 * NotImplemented. Top-level comments page through @p filters like any list;
	 * thread replies come inline on each @ref Comment.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the comment list
	 * @return A page of comments, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<Comment>> comments(
	    RequestorContext context,
	    GetFilters filters);

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

	/**
	 * @brief The search filters and sorts this getter supports.
	 * Cache-first and async: returns the cached table, fetching it from the source
	 * (and caching it, e.g. in RequestorContext::resources()) on a cold cache. A
	 * static-catalog parser just returns its table. Read it once and validate
	 * synchronously via the free @ref validate_query() for UI pre-flight.
	 */
	virtual NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext context);
	virtual MangaGetterRootCompatibilities latest_support() const noexcept;

	/**
	 * @brief Check the requested sort against this getter's own
	 * latest_support(). Same contract as validate_query, for latest().
	 * @return Empty if the filters are valid; otherwise the violation
	 */
	[[nodiscard]] std::vector<SearchQueryError> validate_latest_filters(const GetFilters& filters) const;

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
	 * @brief Autocomplete: suggest search tokens for a partial input.
	 * Advertised via SearchCompatibilities::compatibilities (supports_suggestions);
	 * NotImplemented by default. @p kind optionally narrows to one search axis — a
	 * search key (@see search_keys), the same vocabulary as the source's filters and
	 * as SearchSuggestion::category — best-effort, the source may ignore it. The
	 * returned SearchSuggestion::value is a search token that drops straight back
	 * into search (as that filter's value or a query token).
	 * @param context Client to perform HTTP requests
	 * @param partial The token being typed.
	 * @param kind Optional search axis to complete; nullopt = across the default axis.
	 * @return Suggestions, most relevant first.
	 */
	virtual NetworkRequestTask<std::vector<SearchSuggestion>> suggest(
	    RequestorContext context,
	    std::string partial,
	    std::optional<std::string> kind = std::nullopt);

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