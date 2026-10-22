/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
// The data model half; this header adds the getter interfaces on top of it.
#include "aniparse/anime/AnimeModel.hpp"

#include "aniparse/types/Flags.hpp"
#include "aniparse/types/Pagination.hpp"
#include "aniparse/types/Response.hpp"
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
	 * @brief The works the source itself declares as related to this one — a sequel, a
	 * season, a spin-off, or the story in another medium (the manga it adapts) —
	 * paginated via @p filters. Distinct from similar(): a relation the source asserts,
	 * not a recommendation it computes.
	 *
	 * Returns descriptors, not getters, because a relation may leave this medium and an
	 * AnimeGetter cannot address a manga. @see RelatedWork, MangaGetter::related
	 *
	 * By default reports RequestErrorCode::NotImplemented.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the list
	 * @return A page of related works, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<RelatedWork>> related(
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
	 * @brief Return this anime's stable, public URL.
	 *
	 * The returned absolute URL must route through ParserStore::route_url() and
	 * reconstruct an equivalent getter through CatalogRootGetter::parse_url(). This
	 * method performs no I/O: @p context is supplied only to read the immutable
	 * volatile-catalog snapshot (in particular canonical_base_url), never to make a
	 * request. The getter's own identity supplies the remaining path/query data.
	 * @param context Context carrying the parser-scoped catalog snapshot.
	 * @return An absolute public URL that identifies this anime.
	 */
	[[nodiscard]] virtual std::string canonical_url(RequestorContext context) const = 0;

	/**
	 * @brief This getter's identity in a form that survives the process, so a
	 * consumer can store the anime in a library and rebuild the getter later with
	 * CatalogRootGetter::from_serialized(). Every getter must implement it.
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



} // namespace aniparse
