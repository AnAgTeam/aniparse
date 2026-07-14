/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
// The data model half; this header adds the getter interfaces on top of it.
#include "aniparse/anime/AnimeModel.hpp"
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
 * @brief How one anime getter advertises what it can do.
 *
 * Most AnimeGetter methods are optional: a parser that does not override one
 * inherits a default that reports RequestErrorCode::NotImplemented. The flags here
 * are how a parser declares, without a request, which of them it actually
 * implements — so a consumer consults AnimeGetter::compatibilities() before
 * offering the corresponding action (a track picker, a comment thread) rather than
 * discovering the gap by spending a failed request. The NotImplemented default
 * remains the backstop for a caller that skips the check.
 * @see compatibilities_flags
 */
struct AnimeGetterCompatibilities {
	/// The capability bits this getter claims, drawn from the
	/// aniparse::compatibilities_flags vocabulary (supports_tracks gates
	/// AnimeGetter::tracks, supports_commenting gates AnimeGetter::comments).
	/// Default = nothing optional claimed.
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};

/**
 * @brief How an anime root getter advertises what AnimeRootGetter::latest()
 * accepts. The latest() counterpart of SearchCompatibilities: latest() takes no
 * query and no filter items, only an ordering, so the sort channel is all there is
 * to declare. Available synchronously (no fetch, unlike
 * AnimeRootGetter::search_support), and checked against a request by
 * AnimeRootGetter::validate_latest_filters.
 */
struct AnimeGetterRootCompatibilities {
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
 * @brief Interface for getting one specific anime's information and video.
 * The anime counterpart of MangaGetter: episodes replace chapters, and a
 * chapter's page list becomes an episode's list of playable sources.
 *
 * A getter is IMMUTABLE once constructed: its methods read what the constructor set and
 * never write it. Data known up front goes in the constructor; data fetched at request
 * time goes in RequestorContext::resources(), never into a member. This is what makes
 * concurrent calls on one getter safe without a lock, and callers rely on it.
 * @see MangaGetter for the full statement of the rule.
 */
struct AnimeGetter {
	virtual ~AnimeGetter() = default;

	/**
	 * @brief The optional operations this getter implements.
	 * Every getter must declare them; the answer is fixed at construction, so the
	 * call is synchronous and performs no request. A consumer reads it before
	 * calling any optional method — notably tracks(), which also decides the shape
	 * of its navigation.
	 * @return The capability flags of this getter.
	 */
	virtual AnimeGetterCompatibilities compatibilities() const noexcept = 0;

	/**
	 * @brief The short-card info this getter was handed, if it was handed any.
	 * Free and synchronous — it reads what the constructor set and never requests
	 * anything. Nullopt means "nothing known yet", and the caller decides what that is
	 * worth: a placeholder card, or info() for the full record. @see MangaGetter for
	 * the full statement of the contract.
	 * @return The card info, or nullopt when this getter has none.
	 */
	[[nodiscard]] virtual std::optional<AnimeInfo> preview_info() const noexcept;

	/**
	 * @brief Full metadata of this anime. Every getter must implement it — it is the
	 * one operation no source can omit.
	 * @param context Client to perform HTTP requests
	 * @return The complete AnimeInfo, or a RequestError (NotFound when the item is
	 *         gone, UnexpectedResponse when the source's shape changed, ...)
	 */
	virtual NetworkRequestTask<AnimeInfo> info(RequestorContext context) const = 0;

	/**
	 * @brief The (team × player) tracks available for this anime, paginated via
	 * @p filters. The pre-episode selection axis for track-first sources; a
	 * NotImplemented default suits episode-first sources that surface players per
	 * episode instead. Advertised via compatibilities() (supports_tracks) so a
	 * consumer picks its navigation shape without a speculative request; the
	 * NotImplemented default remains the backstop for callers that ignore the
	 * flag. @see AnimeTrackInfo, compatibilities_flags::supports_tracks
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the track list
	 * @return A page of tracks, each with the AnimeTrackInfo::id that narrows
	 *         episodes_info() and episode_sources(), or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<AnimeTrackInfo>> tracks(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief The episodes of this anime, paginated via @p filters and optionally
	 * narrowed to one @p track. On a track-first source the episode set depends
	 * on the track (a player may carry fewer episodes), and nullopt = the
	 * source's default track; on an episode-first source the list is flat and
	 * @p track is ignored. Mirrors MangaGetter::chapters_info.
	 *
	 * By default reports RequestErrorCode::NotImplemented: a metadata source that
	 * catalogs anime without streaming it has no episode list to give.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the episode list
	 * @param track Restrict to one track, identified by an AnimeTrackInfo::id from
	 *        tracks(); nullopt = the source's default.
	 * @return A page of episode descriptors — each carrying the handle that opens it
	 *         (@see AnimeEpisodeInfo::ref) — or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<AnimeEpisodeInfo>> episodes_info(
	    RequestorContext context,
	    GetFilters filters,
	    std::optional<AnimeTrackID> track = std::nullopt) const;

	/**
	 * @brief User comments on the anime, paginated via @p filters.
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
	 * @brief The anime the source itself declares as related to this one (a sequel,
	 * a season, a spin-off — whichever relation the source states), paginated via
	 * @p filters, each as a ready-to-use getter. Distinct from similar(): a relation
	 * asserted by the source, not a recommendation computed from it. By default
	 * reports RequestErrorCode::NotImplemented.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the list
	 * @return A page of getters for the related anime, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> related(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief The anime the source recommends as similar to this one, paginated via
	 * @p filters, each as a ready-to-use getter. The recommendation axis, as opposed
	 * to the declared relations of related(). By default reports
	 * RequestErrorCode::NotImplemented.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the list
	 * @return A page of getters for the similar anime, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> similar(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief The playable sources for one episode, paginated via @p filters and
	 * optionally narrowed to one @p track. The anime counterpart of
	 * MangaGetter::chapter_pages: each source is one team×player option carrying
	 * a quality ladder. On a track-first source @p track pins the combination, so
	 * this yields a single VideoSource; on an episode-first source @p track is
	 * nullopt and this yields one VideoSource per available player, each
	 * self-describing. A source backed by a native player arrives with its
	 * @ref VideoSource::streams filled; one backed by an external embed may need
	 * @ref resolve_video to expand its stream_url first.
	 * The episode is identified by round-trip: pass AnimeEpisodeInfo::ref().
	 *
	 * Every getter must implement it, but implementing it does not mean streaming:
	 * a metadata source that catalogs anime without serving it overrides this to
	 * report RequestErrorCode::NotImplemented explicitly.
	 * @param context Client to perform HTTP requests
	 * @param episode Identity of the episode to open, from AnimeEpisodeInfo::ref()
	 * @param filters Pagination (and any supported ordering) over the source list
	 * @param track The track the episode belongs to, from tracks(); nullopt on an
	 *        episode-first source, where every available player is returned instead.
	 * @return A page of playable sources, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<VideoSource>> episode_sources(
	    RequestorContext context,
	    AnimeEpisodeRef episode,
	    GetFilters filters,
	    std::optional<AnimeTrackID> track = std::nullopt) const = 0;

	/**
	 * @brief Expand a source into directly-playable streams.
	 * A native-player source needs nothing — the default is an identity
	 * passthrough that returns @p source unchanged. A source whose media sits
	 * behind an external embed overrides this to turn its @ref
	 * VideoSource::stream_url into a populated @ref VideoSource::streams. The
	 * seam mirrors the images fetch_page override; the embed/decrypt paths live
	 * in the parser that needs them, not in this neutral interface.
	 *
	 * Unlike the other optional methods this one does NOT report NotImplemented when
	 * unoverridden — the default succeeds. So it is always safe to call, and a
	 * consumer may route every source through it before playback rather than
	 * deciding per source whether resolution is needed.
	 * @param context Client to perform HTTP requests
	 * @param source A source obtained from episode_sources(), moved in
	 * @return The same source with @ref VideoSource::streams playable, or a
	 *         RequestError when the embed could not be expanded
	 */
	virtual NetworkRequestTask<VideoSource> resolve_video(
	    RequestorContext context,
	    VideoSource source) const;

	/**
	 * @brief This getter's identity in a form that survives the process, so a
	 * consumer can store the anime in a library and rebuild the getter later with
	 * AnimeRootGetter::from_serialized(). Every getter must implement it.
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
 * @brief Root interface for an anime source: search and browse titles.
 * The anime counterpart of MangaRootGetter — same surface, exactly.
 */
struct AnimeRootGetter {
	virtual ~AnimeRootGetter() = default;

	/**
	 * @brief The search filters and sorts this getter supports.
	 * Cache-first and async; validate a query synchronously via the free
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
	virtual AnimeGetterRootCompatibilities latest_support() const noexcept;

	/**
	 * @brief Check the requested sort against this getter's own latest_support().
	 * Same contract as validate_query, for latest().
	 * @param filters The filters about to be passed to latest(); only the sort
	 *        channel is checked, as that is all latest() takes.
	 * @return Empty if the filters are valid; otherwise the violation.
	 */
	[[nodiscard]] std::vector<SearchQueryError> validate_latest_filters(const GetFilters& filters) const;

	/**
	 * @brief Search the source's anime catalog by free text and/or structured filters.
	 * Each result is a ready-to-use getter, typically already carrying its short-card
	 * info. What @p query and @p filters may hold is declared by search_support() and
	 * pre-flighted with validate_query. NotImplemented by default.
	 * @see MangaRootGetter::search
	 * @param context Client to perform HTTP requests
	 * @param query Free text plus the structured filter items (@see SearchRequestQuery)
	 * @param filters Pagination and ordering of the result list
	 * @return A page of anime getters, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters) const;

	/**
	 * @brief The source's recently added or recently updated anime — its front page,
	 * as opposed to an answer to a query. Which orderings @p filters may request is
	 * declared by latest_support(). NotImplemented by default.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination and (where declared) ordering of the list
	 * @return A page of anime getters, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief Autocomplete: suggest search tokens for a partial input.
	 * Advertised via SearchCompatibilities::compatibilities (supports_suggestions);
	 * NotImplemented by default. @see MangaRootGetter::suggest
	 * @param context Client to perform HTTP requests
	 * @param partial The token being typed.
	 * @param kind Optional search axis to complete; nullopt = across the default axis.
	 * @return Suggestions, most relevant first, or a RequestError
	 */
	virtual NetworkRequestTask<std::vector<SearchSuggestion>> suggest(
	    RequestorContext context,
	    std::string partial,
	    std::optional<std::string> kind = std::nullopt) const;

	/**
	 * @brief Parse a url into the anime getter it addresses.
	 * NotImplemented by default. @see MangaRootGetter::parse_url
	 * @param context Client to perform HTTP requests
	 * @param url The url to parse; already split into its parts, and expected to
	 *        belong to this source (a caller routes it there first).
	 * @return A getter for the anime the url addresses, or a RequestError
	 *         (RequestErrorCode::InvalidArguments when the url addresses no anime here)
	 */
	virtual NetworkRequestTask<std::unique_ptr<AnimeGetter>> parse_url(
	    RequestorContext context,
	    ParsedUrl url) const;

	/**
	 * @brief Rebuild an anime getter from the identity AnimeGetter::serialize() emitted
	 * — how a stored library entry becomes usable again after a restart. Every root
	 * getter must implement it, and it must keep accepting every form this parser has
	 * ever emitted (@see SerializedGetterData). The restored getter carries identity
	 * only, no cached info.
	 * @param data The blob a previous serialize() returned, verbatim
	 * @return A getter addressing the same anime, or a RequestError
	 *         (RequestErrorCode::InvalidArguments when the blob does not decode)
	 */
	virtual NetworkRequestTask<std::unique_ptr<AnimeGetter>> from_serialized(SerializedGetterData data) const = 0;
};

} // namespace aniparse
