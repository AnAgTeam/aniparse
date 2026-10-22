/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/anime/AnimeGetter.hpp"

namespace aniparse {

// A getter that was not handed a card knows nothing, and says so. It must not
// fall back to info(): a caller drawing a list would then pay a detail request
// per row without asking for one.
std::optional<AnimeInfo> AnimeGetter::preview_info() const noexcept {
	return std::nullopt;
}

RequestTask<PageResults<AnimeTrackInfo>> AnimeGetter::load_tracks(
    RequestContext,
    GetFilters) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot list tracks");
}

RequestTask<PageResults<AnimeEpisodeInfo>> AnimeGetter::load_episodes(
    RequestContext,
    GetFilters,
    std::optional<AnimeTrackID>) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get episodes info");
}

RequestTask<PageResults<Comment>> AnimeGetter::load_comments(
    RequestContext,
    GetFilters) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get comments");
}

RequestTask<PageResults<RelatedWork>> AnimeGetter::load_related(
    RequestContext,
    GetFilters) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get related info");
}

RequestTask<PageResults<std::unique_ptr<AnimeGetter>>> AnimeGetter::load_similar(
    RequestContext,
    GetFilters) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get similar info");
}

RequestTask<VideoSource> AnimeGetter::resolve_video(
    RequestContext,
    VideoSource source) const {
	// Native players arrive already resolved; the default is an identity
	// passthrough. Embed-backed sources override to expand stream_url.
	co_return source;
}


} // namespace aniparse
