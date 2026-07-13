/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Common.hpp"
#include "aniparse/ParsedUrl.hpp"
// The data model half; this header adds the getter interfaces on top of it.
#include "aniparse/manga/MangaModel.hpp"

namespace aniparse {


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
 *
 * A getter is IMMUTABLE once constructed: its methods read the fields the constructor
 * set and never write them. Anything a getter would otherwise want to remember — a
 * fetched catalog, a warmed snapshot, a preview it was handed — belongs either in the
 * constructor (data known up front, e.g. the short-card info a search result carries) or
 * in RequestorContext::resources() (data fetched at request time, shared and keyed).
 *
 * That is a contract, not an observation: it is what makes concurrent calls on one getter
 * safe without a lock. A consumer may hold a getter and call it from more than one thread,
 * so a getter that caches into a member introduces a data race its caller has no way to
 * see coming.
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