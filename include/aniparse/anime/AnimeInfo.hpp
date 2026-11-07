/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/media/Ids.hpp"
#include "aniparse/media/MediaInfo.hpp"
#include "aniparse/media/RelatedWork.hpp"
#include "aniparse/net/Headers.hpp"
#include "aniparse/video/VideoPlaybackInfo.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/*
 * The anime DATA model, split out of Anime.hpp so it can be included without the
 * getter interfaces — and therefore without coroutines, expected, or the client.
 * Anime.hpp includes this and adds the getters, so nothing else changes.
 *
 * Kept transport-free on purpose: this is the half a consumer binds to (the Swift
 * bridge imports it directly), and a header that reaches RequestTask cannot
 * be imported by Swift's clang importer at all. @see MangaModel.hpp
 */

/**
 * @file
 * The anime data model: an anime, its (team × player) tracks, its episodes, and the
 * playable sources of an episode — the values an AnimeGetter returns.
 *
 * It parallels the manga model deliberately (episodes for chapters, playable sources
 * for pages) and follows the same two rules: an absent value (an empty string, a
 * nullopt, @c nullopt) means the source did not state it in that
 * response, not that the anime lacks it; and identity is the opaque handle
 * (AnimeEpisodeInfo::ref()), not the episode number, which is for display.
 *
 * What has no counterpart in manga is the pre-episode axis: a source may offer the
 * same episode from several teams on several players. Where it does, that axis is
 * @ref aniparse::AnimeTrackInfo; where it does not, each @ref aniparse::VideoSource
 * describes itself instead.
 */

namespace aniparse {

/**
 * The source's opaque identity key for an anime: a numeric ID encoded as text,
 * a slug, or another stable source-local key. Prefer the most stable part of the
 * source identity; exclude mirror hosts, mutable title text, and URL decoration
 * when a durable key is available. Never hash a slug merely to obtain a number.
 * Meaningful only within the issuing source, catalog and media kind, never
 * across parsers (@see ExternalId). Consumers must not parse it or infer ordering from it.
 * Persist the getter's serialized data to restore access, not this key alone.
 * @see AnimeInfo::id
 */
using AnimeID = std::string;
/**
 * The source's handle for one (team × player) track of an anime, as advertised by
 * @ref aniparse::AnimeTrackInfo::id. Meaningful only within that source and that anime; it is
 * what an episode listing or a source fetch is narrowed by. @see AnimeTrackInfo
 */
using AnimeTrackID = int;
/// A number of episodes. Counts only, never an index: the first episode is 1.
using EpisodeCount = int;

struct AnimeGetter;

/// The AnimeTrackID that addresses no particular track — the default of
/// @ref aniparse::AnimeTrackInfo::id, and what an episode-first source (which has no track
/// axis at all) leaves it at. Getters express the same "no track chosen" by passing
/// nullopt, which selects the source's default.
inline constexpr AnimeTrackID any_anime_track = -1;

/**
 * @brief The broadcast season an anime premiered in — the industry's coarse release
 * slot, paired with @ref aniparse::AnimeInfo::year (a season alone does not date anything).
 */
enum class AnimeSeason {
	Unknown, ///< The source states no season. Distinct from an absent season field: it means the source has the axis but no value for this anime.
	Spring,  ///< Roughly April-June.
	Summer,  ///< Roughly July-September.
	Fall,    ///< Roughly October-December.
	Winter,  ///< Roughly January-March.
};

/**
 * @brief A future episode a source schedules for an anime.
 *
 * This is deliberately not an @ref AnimeEpisodeInfo — a scheduled episode may
 * not yet exist on the source, so it has neither a fetchable identity nor
 * playable sources. Empty @ref number or @ref name means the source announced
 * only the other value or only the release time.
 */
struct UpcomingEpisodeInfo {
	/// Episode label exactly as the source states it, such as "12", "7.5", or
	/// "OVA". Empty = the source does not identify the scheduled episode.
	std::string number;
	/// Scheduled episode title. Empty = none stated; it does not repeat @ref number.
	std::string name;
	/// When this episode is expected to air or be posted. @c nullopt = the source
	/// announced the episode but not a date. @see ModelDate for date precision.
	std::optional<ModelDate> release_time;
};

/**
 * @brief Metadata of one anime. The anime counterpart of MangaInfo /
 * ImageContainerInfo: a plain data model returned by AnimeGetter::load_info, not a
 * bag of getter methods. Dub/fansub teams are not fields here — they are a
 * separate axis, @see AnimeGetter::load_translations.
 *
 * The same struct serves two fetch depths: a listing card (AnimeGetter::preview_info)
 * fills only what a cheap listing endpoint carries, a full fetch fills what the
 * detail endpoint carries. An empty string or a nullopt therefore means "not stated
 * in this response", not "the anime does not have it".
 */
struct AnimeInfo {
	/// Opaque source-local identity; numeric IDs are represented as decimal text.
	/// Use the most stable source key available, including a slug when no numeric
	/// key exists. Empty = identity unavailable in this response; never use an
	/// empty key for deduplication. Not a fetch handle (@see AnimeGetter::serialize).
	AnimeID id;

	/// The metadata shared with every other domain — title, description, dates, tags,
	/// series, previews, rating, external ids, and the rest. @see aniparse::MediaInfo
	MediaInfo common;
	/// Promotional stills or screenshots from this anime. Distinct from
	/// @ref MediaInfo::previews, which are the work's cover/poster artwork. Empty =
	/// the source offers no separate stills in this response. Fetch descriptors,
	/// not image bytes. @see Image
	std::vector<Image> screenshots;

	/// @deprecated Use MediaInfo::release_time with DatePrecision::Quarter for an
	/// actual premiere, or UpcomingEpisodeInfo::release_time for an announced one.
	/// The broadcast season it premiered in. nullopt = the source has no season axis
	/// at all; AnimeSeason::Unknown = it has one but states no value for this anime.
	/// Only meaningful together with @ref year. @see AnimeSeason
	std::optional<AnimeSeason> season;
	/// @deprecated Use MediaInfo::release_time with DatePrecision::Year or
	/// DatePrecision::Quarter for an actual premiere, or
	/// UpcomingEpisodeInfo::release_time for an announced one. A default-constructed
	/// year (year 0) = the source states none — compare against
	/// `std::chrono::year{}`, since year 0 is otherwise a well-formed value.
	std::chrono::year year{};

	/// Episodes actually available now (a running airing exposes fewer than
	/// planned); absent when the source does not state it.
	std::optional<EpisodeCount> released_episodes;
	/// Planned episode total when the source states it up front; absent otherwise.
	std::optional<long> total_episodes;
	/// The next scheduled episode, when the source publishes one. It may be absent
	/// for an ongoing anime, because the library never infers a date or number from
	/// cadence or from @ref released_episodes. @see UpcomingEpisodeInfo
	std::optional<UpcomingEpisodeInfo> next_episode;
	/// Nominal runtime of one episode, as the source states it — a typical value for
	/// the series, not a per-episode measurement, so it is an estimate for a UI and
	/// not a seek/progress bound. nullopt = the source does not state it.
	std::optional<std::chrono::minutes> episode_duration;
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
 * leave load_tracks() NotImplemented and self-describe on @ref VideoSource instead.
 */
struct AnimeTrackInfo {
	/// Opaque handle for this (team × player) combination; round-trips into
	/// load_episodes() / load_episode_sources().
	AnimeTrackID id = any_anime_track;
	RelatedUser team;                    ///< The dub/fansub group.
	/// Whether this track is a spoken translation or timed text — the other half of
	/// the user-facing choice next to the team. Defaults to Voiceover, so a source
	/// that carries subtitles must set it. @see TranslationType
	TranslationType type = TranslationType::Voiceover;
	std::string player;                  ///< Host/player label; opaque routing token.
	/// Episodes available under THIS track; absent when the source does not
	/// state it up front.
	std::optional<EpisodeCount> episode_count;
};

/**
 * @brief Identity of one episode: everything load_episode_sources() needs and
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

/**
 * @brief Everything an episode listing states about one episode. The item type of
 * AnimeGetter::load_episodes; its @ref ref() is what fetches the episode's playable
 * sources. The anime counterpart of MangaChapterInfo.
 *
 * An episode's identity is the ref, never the number: the numeric field is for
 * grouping and display, and specials or recaps may share or lack one.
 */
struct AnimeEpisodeInfo {
	/// Numeric hint for grouping/ordering in UI; not the episode's identity.
	/// Whole part only, 0 = unnumbered (a special), so not a usable sort key alone.
	long episode = 0;
	/// Source episode number without a generic presentation label: "7.5", "10",
	/// or a non-numeric special marker such as "OVA". Do not include words such
	/// as "episode", "series", or their localized equivalents: those belong to
	/// the consuming UI. Empty = render from @ref episode.
	std::string number;
	/// Opaque episode handle, understood only by the getter that produced this
	/// info; round-trips into load_episode_sources() via ref().
	std::string id;
	/// The episode's own title, when it has one. Empty = untitled (or not carried in
	/// a cheap listing); it does not repeat the episode number, so a UI shows both.
	std::string name;
	/// Synopsis for the episode. Empty = none, the usual case.
	std::string description;
	/// Thumbnails/stills for the episode. Empty = the source offers none. Fetch
	/// descriptors, not the video — that comes from load_episode_sources(). @see Image
	std::vector<Image> previews;
	/// When the episode entry was last edited/re-uploaded on the source;
	/// @c nullopt = not stated.
	std::optional<ModelDate> update_time;
	/// When the episode aired or was posted; @c nullopt = not stated. Sources
	/// differ on which of the two they report, so it dates the entry, not the broadcast.
	std::optional<ModelDate> release_time;

	/**
	 * @brief Identity for the load_episode_sources() round-trip.
	 * @return The ref built from this info's number and opaque id. Pass it back
	 *         unchanged — the number alone identifies an episode only on sources that
	 *         leave @ref id empty. Cheap: nothing is fetched, the id is copied.
	 */
	[[nodiscard]] AnimeEpisodeRef ref() const { return { .episode = episode, .id = id }; }
};

/**
 * @brief One selectable playback option for an episode: a team's track on a
 * given player, carrying its quality ladder. Replaces the flat MangaPage list
 * of the manga domain — an episode returns several of these (team × player ×
 * quality-set), and the user picks one.
 *
 * The @ref VideoPlaybackInfo::streams may be empty until the source is resolved: a native player
 * returns them directly, an external embed only yields a @ref stream_url that
 * resolve_video() must expand. @see AnimeGetter::episode_sources, resolve_video
 */
struct VideoSource {
	/// Player/host that serves the media, e.g. a site's own player or an
	/// external embed. Routes native-passthrough vs resolve. Opaque to consumers.
	std::string player;
	/// Whether this option is a spoken translation or timed text. Defaults to
	/// Voiceover, so a source offering subtitles must set it; on a track-first source
	/// it repeats the track's AnimeTrackInfo::type, so the option is readable on its
	/// own. @see TranslationType
	TranslationType translation_type = TranslationType::Voiceover;
	/// The dub/fansub group behind this source — self-describing, so an
	/// episode-first source (no track axis) still names its team here.
	RelatedUser team;
	/// Backlink to the AnimeTrackInfo::id this source belongs to, when the source
	/// exposes the track axis; absent in episode-first (untracked) mode.
	std::optional<AnimeTrackID> track;

	/// For an external embed: the page/embed URL resolve_video() expands into
	/// @ref VideoPlaybackInfo::streams. Empty when @ref VideoPlaybackInfo::streams is already populated (native player).
	std::string stream_url;
	/// Direct media resources and their access policy.
	VideoPlaybackInfo playback;

	/// A poster/still for this option, when the source offers one distinct from
	/// the anime's previews.
	std::optional<Image> poster;

};

} // namespace aniparse
