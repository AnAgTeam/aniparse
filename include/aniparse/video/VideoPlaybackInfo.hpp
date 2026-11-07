#pragma once
#include "aniparse/video/VideoStream.hpp"
#include "aniparse/net/Headers.hpp"
#include <memory>
#include <vector>

namespace aniparse {
class ResourceAdapter;

/**
 * @brief Playable media resources shared by sources and extraction results.
 * Empty streams mean no directly playable formats are currently available;
 * this is not a resolution-state flag. Streams may contain muxed audio/video.
 */
struct VideoPlaybackInfo {
    /// Directly playable formats, in preferred order.
    std::vector<VideoStream> streams;
    /// Separate timed-text files; alternative formats may represent one translation.
    std::vector<VideoSubtitle> subtitles;
    /// Request headers for media resources, not for the embed page.
    Headers headers;
    /// Optional request and byte transformation policy for media resources.
    std::shared_ptr<const ResourceAdapter> adapter;

    /**
     * @brief Preserve resources and policies from the containing source or extractor.
     * Current streams and headers take precedence; missing subtitle URLs are appended.
     * Adapters compose with the fallback as the outer policy.
     * @param fallback Previously known resources, consumed by this operation.
     */
    void merge_fallback(VideoPlaybackInfo fallback);
};
} // namespace aniparse
