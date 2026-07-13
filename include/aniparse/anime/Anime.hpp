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

struct AnimeGetterCompatibilities {
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};

struct AnimeGetterRootCompatibilities {
	SupportedSorts supported_sorts;
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

	virtual AnimeGetterCompatibilities compatibilities() const noexcept = 0;

	virtual NetworkRequestTask<AnimeInfo> preview_info(RequestorContext context);

	virtual NetworkRequestTask<AnimeInfo> info(RequestorContext context) = 0;

	/**
	 * @brief The (team × player) tracks available for this anime, paginated via
	 * @p filters. The pre-episode selection axis for track-first sources; a
	 * NotImplemented default suits episode-first sources that surface players per
	 * episode instead. Advertised via compatibilities() (supports_tracks) so a
	 * consumer picks its navigation shape without a speculative request; the
	 * NotImplemented default remains the backstop for callers that ignore the
	 * flag. @see AnimeTrackInfo, compatibilities_flags::supports_tracks
	 */
	virtual NetworkRequestTask<PageResults<AnimeTrackInfo>> tracks(
	    RequestorContext context,
	    GetFilters filters);

	/**
	 * @brief The episodes of this anime, paginated via @p filters and optionally
	 * narrowed to one @p track. On a track-first source the episode set depends
	 * on the track (a player may carry fewer episodes), and nullopt = the
	 * source's default track; on an episode-first source the list is flat and
	 * @p track is ignored. Mirrors MangaGetter::chapters_info.
	 */
	virtual NetworkRequestTask<PageResults<AnimeEpisodeInfo>> episodes_info(
	    RequestorContext context,
	    GetFilters filters,
	    std::optional<AnimeTrackID> track = std::nullopt);

	/**
	 * @brief User comments on the anime, paginated via @p filters.
	 * Available when the parser advertises supports_commenting; NotImplemented
	 * by default. @see MangaGetter::comments
	 */
	virtual NetworkRequestTask<PageResults<Comment>> comments(
	    RequestorContext context,
	    GetFilters filters);

	virtual NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> related(
	    RequestorContext context,
	    GetFilters filters);

	virtual NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> similar(
	    RequestorContext context,
	    GetFilters filters);

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
	 */
	virtual NetworkRequestTask<PageResults<VideoSource>> episode_sources(
	    RequestorContext context,
	    AnimeEpisodeRef episode,
	    GetFilters filters,
	    std::optional<AnimeTrackID> track = std::nullopt) = 0;

	/**
	 * @brief Expand a source into directly-playable streams.
	 * A native-player source needs nothing — the default is an identity
	 * passthrough that returns @p source unchanged. A source whose media sits
	 * behind an external embed overrides this to turn its @ref
	 * VideoSource::stream_url into a populated @ref VideoSource::streams. The
	 * seam mirrors the images fetch_page override; the embed/decrypt paths live
	 * in the parser that needs them, not in this neutral interface.
	 */
	virtual NetworkRequestTask<VideoSource> resolve_video(
	    RequestorContext context,
	    VideoSource source);

	virtual NetworkRequestTask<SerializedGetterData> serialize() = 0;
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
	 * validate_query. @see MangaRootGetter::search_support
	 */
	virtual NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext context);
	virtual AnimeGetterRootCompatibilities latest_support() const noexcept;

	/**
	 * @brief Check the requested sort against this getter's own latest_support().
	 * Same contract as validate_query, for latest().
	 * @return Empty if the filters are valid; otherwise the violation.
	 */
	[[nodiscard]] std::vector<SearchQueryError> validate_latest_filters(const GetFilters& filters) const;

	/**
	 * @brief Search anime by query and/or filters. NotImplemented by default.
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters);

	/**
	 * @brief Latest anime from the source. NotImplemented by default.
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters);

	/**
	 * @brief Autocomplete: suggest search tokens for a partial input.
	 * Advertised via SearchCompatibilities::compatibilities (supports_suggestions);
	 * NotImplemented by default. @see MangaRootGetter::suggest
	 */
	virtual NetworkRequestTask<std::vector<SearchSuggestion>> suggest(
	    RequestorContext context,
	    std::string partial,
	    std::optional<std::string> kind = std::nullopt);

	/**
	 * @brief Parse a url into the anime getter it addresses.
	 * NotImplemented by default. @see MangaRootGetter::parse_url
	 */
	virtual NetworkRequestTask<std::unique_ptr<AnimeGetter>> parse_url(
	    RequestorContext context,
	    ParsedUrl url);

	/**
	 * @brief Getter for serialized data from one of serialize() methods.
	 */
	virtual NetworkRequestTask<std::unique_ptr<AnimeGetter>> from_serialized(SerializedGetterData data) = 0;
};

} // namespace aniparse
