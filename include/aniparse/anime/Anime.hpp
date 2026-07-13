/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Common.hpp"
#include "aniparse/ParsedUrl.hpp"

namespace aniparse {

using AnimeID      = int64_t;
using AnimeTrackID = int;
using EpisodeCount = int;

struct AnimeGetter;

inline constexpr AnimeID invalid_anime_id     = AnimeID{ 0 };
inline constexpr AnimeTrackID any_anime_track = -1;

enum class AnimeSeason {
	Unknown,
	Spring,
	Summer,
	Fall,
	Winter,
};

/**
 * @brief Whether a video source carries a spoken translation or timed text.
 * The user-facing axis alongside the team: "the Voiceover from group X" vs
 * "Subtitles from group Y". @see VideoSource, AnimeTrackInfo
 */
enum class TranslationType {
	Voiceover, ///< A dub / voice-over track baked into (or muxed with) the stream.
	Subtitles, ///< The original audio with a timed-text track (soft or burned).
};

/**
 * @brief Metadata of one anime. The anime counterpart of MangaInfo /
 * ImageContainerInfo: a plain data model returned by AnimeGetter::info, not a
 * bag of getter methods. Dub/fansub teams are not fields here — they are a
 * separate axis, @see AnimeGetter::translation_info.
 */
struct AnimeInfo {
	AnimeID id = invalid_anime_id;

	std::string title;
	std::optional<std::string> original_title;
	AttributedText description;

	std::optional<AnimeSeason> season;
	std::chrono::year year{};
	AiredStatus status;

	std::chrono::system_clock::time_point update_time  = unknown_time;
	std::chrono::system_clock::time_point release_time = unknown_time;

	/// Opaque change marker for the whole anime; @see MangaInfo::revision.
	std::string revision;

	std::optional<Series> series;

	std::vector<Image> previews;
	std::vector<Tag> tags;

	std::optional<Rating> rating;
	std::optional<ViewStats> views;

	/// Episodes actually available now (a running airing exposes fewer than
	/// planned); absent when the source does not state it.
	std::optional<EpisodeCount> released_episodes;
	/// Planned episode total when the source states it up front; absent otherwise.
	std::optional<long> total_episodes;
	std::optional<std::chrono::minutes> episode_duration;

	AgeRestriction age_restriction = 0;

	std::optional<RelatedUser> uploader;

	bool is_hentai = false;
};

/**
 * @brief One browsable way to watch: a dub/fansub team on a specific player.
 * The pre-episode selection axis. Its unit is the (team × player) pair, not the
 * team alone, because the player also gates episode availability — the same dub
 * can offer a different episode set on two players (e.g. 10 on one host, 9 on
 * another). So each track carries its own @ref episode_count, and a source that
 * exposes the axis lists exactly the valid combinations.
 *
 * Optional: episode-first sources (players discovered per episode, not up front)
 * leave tracks() NotImplemented and self-describe on @ref VideoSource instead.
 */
struct AnimeTrackInfo {
	/// Opaque handle for this (team × player) combination; round-trips into
	/// episodes_info() / episode_sources().
	AnimeTrackID id = any_anime_track;
	RelatedUser team;                    ///< The dub/fansub group.
	TranslationType type = TranslationType::Voiceover;
	std::string player;                  ///< Host/player label; opaque routing token.
	/// Episodes available under THIS track; absent when the source does not
	/// state it up front.
	std::optional<EpisodeCount> episode_count;
};

/**
 * @brief Identity of one episode: everything episode_sources() needs and
 * nothing else, so the call stays cheap. Obtained via AnimeEpisodeInfo::ref()
 * and round-tripped unchanged; for simply-numbered sources it can also be
 * built directly, e.g. AnimeEpisodeRef{ .episode = 12 }.
 * Mirrors MangaChapterRef (minus the volume — anime episodes are flat).
 */
struct AnimeEpisodeRef {
	/// Episode number; the episode's identity for simply-numbered sources.
	long episode = 0;
	/// Opaque handle from the getter that produced the info.
	/// Empty = the getter identifies the episode by @ref AnimeEpisodeRef::episode.
	std::string id;
};

struct AnimeEpisodeInfo {
	/// Numeric hint for grouping/ordering in UI; not the episode's identity.
	long episode = 0;
	/// Episode number exactly as the source spells it: "7.5", "Special".
	/// Empty = render from @ref episode.
	std::string number;
	/// Opaque episode handle, understood only by the getter that produced this
	/// info; round-trips into episode_sources() via ref().
	std::string id;
	std::string name;
	std::string description;
	std::vector<Image> previews;
	std::chrono::system_clock::time_point update_time  = unknown_time;
	std::chrono::system_clock::time_point release_time = unknown_time;

	/// Identity for the episode_sources() round-trip.
	[[nodiscard]] AnimeEpisodeRef ref() const { return { episode, id }; }
};

/**
 * @brief One technical rung of a source: a fetchable video URL at a given
 * quality. The leaf of the anime domain, the video counterpart of MangaPage —
 * but a source offers a set of these (a quality ladder), not just one.
 * The fetch headers live one level up on @ref VideoSource, shared across rungs.
 */
struct VideoStream {
	/// A direct media URL (progressive mp4 or an HLS playlist) ready to play,
	/// once the containing source has been resolved. @see AnimeGetter::resolve_video
	std::string url;
	/// Vertical resolution in pixels (360/480/720/1080); 0 = unknown/adaptive.
	int quality = 0;
	/// Average bitrate for adaptive selection, when the source states it.
	std::optional<long> bitrate;
	/// The url is an HLS playlist rather than a single progressive file.
	bool is_hls = false;
};

/**
 * @brief One selectable playback option for an episode: a team's track on a
 * given player, carrying its quality ladder. Replaces the flat MangaPage list
 * of the manga domain — an episode returns several of these (team × player ×
 * quality-set), and the user picks one.
 *
 * The @ref streams may be empty until the source is resolved: a native player
 * returns them directly, an external embed only yields a @ref stream_url that
 * resolve_video() must expand. @see AnimeGetter::episode_sources, resolve_video
 */
struct VideoSource {
	/// Player/host that serves the media, e.g. a site's own player or an
	/// external embed. Routes native-passthrough vs resolve. Opaque to consumers.
	std::string player;
	TranslationType translation_type = TranslationType::Voiceover;
	/// The dub/fansub group behind this source — self-describing, so an
	/// episode-first source (no track axis) still names its team here.
	RelatedUser team;
	/// Backlink to the AnimeTrackInfo::id this source belongs to, when the source
	/// exposes the track axis; absent in episode-first (untracked) mode.
	std::optional<AnimeTrackID> track;

	/// For an external embed: the page/embed URL resolve_video() expands into
	/// @ref streams. Empty when @ref streams is already populated (native player).
	std::string stream_url;
	/// The quality ladder, most-preferred first. Populated directly by a native
	/// player, or by resolve_video() for an embed.
	std::vector<VideoStream> streams;

	/// Extra request headers the consumer must send when fetching any of this
	/// source's @ref streams — a Referer/UA/cookies bundle some hosts require to
	/// serve the media (a bare GET is refused otherwise). Shared across the
	/// ladder; empty when the plain URLs suffice. @see Image::headers
	Headers headers;

	/// A poster/still for this option, when the source offers one distinct from
	/// the anime's previews.
	std::optional<Image> poster;
};

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
