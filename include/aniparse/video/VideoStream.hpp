/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <optional>
#include <string>

/*
 * The shared video-playback leaf: the domain-neutral half of a playable source.
 * A quality rung (VideoStream) and the translation axis (TranslationType) are the
 * same problem wherever media plays — the anime domain uses them today, a generic
 * video domain will reuse them unchanged. Kept transport-free (no getters, no
 * RequestTask) like the other data models, so the Swift bridge can bind it.
 */
namespace aniparse {

/**
 * @brief Whether a video source carries a spoken translation or timed text.
 * The user-facing axis alongside the team: "the Voiceover from group X" vs
 * "Subtitles from group Y". @see VideoStream, AnimeTrackInfo
 */
enum class TranslationType {
	Voiceover, ///< A dub / voice-over track baked into (or muxed with) the stream.
	Subtitles, ///< The original audio with a timed-text track (soft or burned).
};

/**
 * @brief One technical rung of a source: a fetchable video URL at a given
 * quality. The leaf of any video domain — a source offers a set of these (a
 * quality ladder), not just one. The fetch headers live one level up on the
 * containing source, shared across rungs.
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
 * @brief A source-provided sidecar subtitle file, separate from the video bytes.
 *
 * Consumers select a supported format; alternative files may describe the same
 * translation in different formats. Empty labels or languages remain unspecified.
 */
struct VideoSubtitle {
	std::string url;      ///< Absolute subtitle file URL.
	std::string format;   ///< Source format token, such as vtt or ass.
	std::string label;    ///< Source-provided display label, when available.
	std::string language; ///< Language code, when explicitly stated by the source.
};

} // namespace aniparse
