/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
// The data model half; this header adds the getter interfaces on top of it.
#include "aniparse/images/ImageModel.hpp"
#include "aniparse/types/ParsedUrl.hpp"

#include "aniparse/types/Flags.hpp"
#include "aniparse/types/Pagination.hpp"
#include "aniparse/types/Response.hpp"
#include "aniparse/types/Search.hpp"
#include "aniparse/types/Serialization.hpp"
#include "aniparse/types/Authentication.hpp"
#include "aniparse/ClientContext.hpp"

namespace aniparse {

/**
 * @brief How one container getter advertises what it can do.
 *
 * The optional ImageContainerGetter methods (comments()) report
 * RequestErrorCode::NotImplemented unless a parser overrides them. The flags here
 * are how a parser declares, without a request, which of them it actually
 * implements — so a consumer consults ImageContainerGetter::compatibilities()
 * before offering the corresponding action rather than discovering the gap by
 * spending a failed request. The NotImplemented default remains the backstop for a
 * caller that skips the check.
 * @see compatibilities_flags
 */
struct ImageContainerCompatibilities {
	/// The capability bits this getter claims, drawn from the
	/// aniparse::compatibilities_flags vocabulary (e.g. supports_commenting gates
	/// ImageContainerGetter::comments). Default = nothing optional claimed.
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};

/**
 * @brief How an images root getter advertises what ImagesGetter::latest() accepts.
 * The latest() counterpart of SearchCompatibilities: latest() takes no query and no
 * filter items, only an ordering, so the sort channel is all there is to declare.
 * Available synchronously (no fetch, unlike ImagesGetter::search_support), and
 * checked against a request by ImagesGetter::validate_latest_filters.
 */
struct ImagesGetterRootCompatibilities {
	/// The orderings latest() accepts on GetFilters::sort, as key -> allowed
	/// directions. Empty (the default) = latest() offers no selectable ordering:
	/// requesting any sort is a SearchQueryError, and the source's own order applies.
	SupportedSorts supported_sorts;
	/// Capability bits that apply to latest(), from the
	/// aniparse::compatibilities_flags vocabulary (e.g. supports_pagination_uniqueness,
	/// which tells a consumer whether paging the list can repeat items). Default = none.
	CompatibilitiesFlags compatibilities = compatibilities_flags::default_flags;
};

/**
 * @brief One container of media: a single booru post or a whole gallery.
 * The images counterpart of MangaGetter.
 *
 * A getter is IMMUTABLE once constructed: its methods read what the constructor set and
 * never write it. Data known up front goes in the constructor; data fetched at request
 * time goes in RequestorContext::resources(), never into a member. This is what makes
 * concurrent calls on one getter safe without a lock, and callers rely on it.
 * @see MangaGetter for the full statement of the rule.
 */
struct ImageContainerGetter {
	virtual ~ImageContainerGetter() = default;

	/**
	 * @brief The optional operations this getter implements.
	 * Every getter must declare them; the answer is fixed at construction, so the
	 * call is synchronous and performs no request. A consumer reads it before
	 * calling any optional method.
	 * @return The capability flags of this getter.
	 */
	virtual ImageContainerCompatibilities compatibilities() const noexcept = 0;

	/**
	 * @brief The short-card info this getter was handed, if it was handed any.
	 * Free and synchronous — it reads what the constructor set and never requests
	 * anything. Nullopt means "nothing known yet", and the caller decides what that is
	 * worth: a placeholder card, or info() for the full record. @see MangaGetter for
	 * the full statement of the contract.
	 * @return The card info, or nullopt when this getter has none.
	 */
	[[nodiscard]] virtual std::optional<ImageContainerInfo> preview_info() const noexcept;

	/**
	 * @brief Full metadata of this container. Every getter must implement it.
	 * @param context Client to perform HTTP requests
	 * @return The complete ImageContainerInfo, or a RequestError (NotFound when the
	 *         container is gone, UnexpectedResponse when the source's shape changed, ...)
	 */
	virtual NetworkRequestTask<ImageContainerInfo> info(RequestorContext context) const = 0;

	/**
	 * @brief The media items of this container, paginated via @p filters.
	 * A flat single-media source returns one item; a gallery pages through
	 * many. Mirrors MangaGetter::chapter_pages, minus the chapter ref — a
	 * container addresses its own media directly. Every getter must implement it:
	 * it is the read path of the images domain.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) over the item list
	 * @return A page of ImageItem fetch descriptors, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<ImageItem>> items(
	    RequestorContext context,
	    GetFilters filters) const = 0;

	/**
	 * @brief User comments on the container, paginated via @p filters.
	 * Available when the parser advertises supports_commenting; NotImplemented
	 * by default. @see MangaGetter::comments
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the comment list
	 * @return A page of comments, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<Comment>> comments(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief This getter's identity in a form that survives the process, so a
	 * consumer can store the container in a library and rebuild the getter later with
	 * ImagesGetter::from_serialized(). Every getter must implement it.
	 *
	 * Encodes identity only, never the fetched info — a restored getter refetches.
	 * The blob is opaque to the consumer and means nothing away from the parser that
	 * produced it. @see SerializedGetterData for the durability contract it carries.
	 *
	 * Coroutine-returning for uniformity with the rest of the interface; an
	 * implementation answers from what its constructor was given and normally
	 * performs no request.
	 * @return The serialized identity, or a RequestError
	 */
	virtual NetworkRequestTask<SerializedGetterData> serialize() const = 0;
};

/**
 * @brief Root interface for an image source: search and browse containers.
 * The images counterpart of MangaRootGetter.
 */
struct ImagesGetter {
	virtual ~ImagesGetter() = default;

	/**
	 * @brief The search filters and sorts this source supports.
	 * Cache-first and async; validate a query against it with the free
	 * validate_query. By default returns an empty table — no filter, no sort, no
	 * flag — so validate_query rejects everything until a parser overrides it.
	 * @see MangaRootGetter::search_support
	 * @param context Client to perform HTTP requests
	 * @return The support table, or a RequestError when it had to be fetched and the
	 *         fetch failed
	 */
	virtual NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext context) const;

	/**
	 * @brief What latest() accepts: its sort declaration and capability flags.
	 * Synchronous, unlike search_support() — the table is fixed, not fetched. By
	 * default returns an empty table, i.e. latest() takes no selectable ordering.
	 * @return The declaration to check a latest() request against.
	 * @see validate_latest_filters
	 */
	virtual ImagesGetterRootCompatibilities latest_support() const noexcept;

	/**
	 * @brief Check the requested sort against this getter's own latest_support().
	 * Same contract as validate_query, for latest().
	 * @param filters The filters about to be passed to latest(); only the sort
	 *        channel is checked, as that is all latest() takes.
	 * @return Empty if the filters are valid; otherwise the violation.
	 */
	[[nodiscard]] std::vector<SearchQueryError> validate_latest_filters(const GetFilters& filters) const;

	/**
	 * @brief Search containers by query and/or filters (e.g. by tag on a tagged
	 * catalog). Each result is a ready-to-use container getter, typically already
	 * carrying the info the listing returned. What @p query and @p filters may hold
	 * is declared by search_support() and pre-flighted with validate_query.
	 * NotImplemented by default.
	 * @param context Client to perform HTTP requests
	 * @param query Free text plus the structured filter items (@see SearchRequestQuery)
	 * @param filters Pagination and ordering of the result list
	 * @return A page of container getters, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters) const;

	/**
	 * @brief The source's most recent containers — its front page, as opposed to an
	 * answer to a query. Which orderings @p filters may request is declared by
	 * latest_support(). NotImplemented by default.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination and (where declared) ordering of the list
	 * @return A page of container getters, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters) const;

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
	    std::optional<std::string> kind = std::nullopt) const;

	/**
	 * @brief Parse a url into the container getter it addresses.
	 * NotImplemented by default. @see MangaRootGetter::parse_url
	 * @param context Client to perform HTTP requests
	 * @param url The url to parse; already split into its parts, and expected to
	 *        belong to this source (a caller routes it there first). A source with
	 *        several container shapes (a single post, a whole pool) decides here
	 *        which one the url names.
	 * @return A getter for the container the url addresses, or a RequestError
	 *         (RequestErrorCode::InvalidArguments when the url addresses no container here)
	 */
	virtual NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> parse_url(
	    RequestorContext context,
	    ParsedUrl url) const;

	/**
	 * @brief Rebuild a container getter from the identity
	 * ImageContainerGetter::serialize() emitted — how a stored library entry becomes
	 * usable again after a restart. Every root getter must implement it, and it must
	 * keep accepting every form this parser has ever emitted (@see
	 * SerializedGetterData). The restored getter carries identity only, no cached info.
	 *
	 * This remains coroutine-returning to preserve one asynchronous compatibility
	 * boundary for every identity format a parser has shipped. Normal restoration
	 * must be local: it decodes @p data, performs no network request, and does not
	 * populate preview data. A parser that must migrate or resolve a legacy identity
	 * may suspend, but that is an exception rather than a reason to fetch a record.
	 * @param data The blob a previous serialize() returned, verbatim
	 * @return A getter addressing the same container, or a RequestError
	 *         (RequestErrorCode::InvalidArguments when the blob does not decode)
	 */
	virtual NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> from_serialized(SerializedGetterData data) const = 0;
};

} // namespace aniparse
