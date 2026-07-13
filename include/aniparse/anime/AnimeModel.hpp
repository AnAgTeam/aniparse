/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Ids.hpp"
#include "aniparse/types/Model.hpp"
#include "aniparse/types/Text.hpp"
#include "aniparse/types/Headers.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

/*
 * The anime DATA model, split out of Anime.hpp so it can be included without the
 * getter interfaces — and therefore without coroutines, expected, or the client.
 * Anime.hpp includes this and adds the getters, so nothing else changes.
 *
 * Kept transport-free on purpose: this is the half a consumer binds to (the Swift
 * bridge imports it directly), and a header that reaches NetworkRequestTask cannot
 * be imported by Swift's clang importer at all. @see MangaModel.hpp
 */
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

} // namespace aniparse
