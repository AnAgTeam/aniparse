#include "aniparse/video/VideoPlaybackInfo.hpp"
#include "aniparse/media/ResourceAdapter.hpp"
#include <algorithm>
#include <utility>

namespace aniparse {
void VideoPlaybackInfo::merge_fallback(VideoPlaybackInfo fallback) {
    if (streams.empty()) streams = std::move(fallback.streams);
    for (auto& subtitle : fallback.subtitles) {
        const bool present = std::ranges::any_of(subtitles, [&](const auto& candidate) {
            return candidate.url == subtitle.url;
        });
        if (!present) subtitles.push_back(std::move(subtitle));
    }
    headers.merge_missing(fallback.headers);
    adapter = ResourceAdapter::compose({std::move(fallback.adapter), std::move(adapter)});
}
} // namespace aniparse
