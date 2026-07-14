/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/anime/Anime.hpp"

namespace aniparse {

// A getter that was not handed a card knows nothing, and says so. It must not
// fall back to info(): a caller drawing a list would then pay a detail request
// per row without asking for one.
std::optional<AnimeInfo> AnimeGetter::preview_info() const noexcept {
	return std::nullopt;
}

NetworkRequestTask<PageResults<AnimeTrackInfo>> AnimeGetter::tracks(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot list tracks");
}

NetworkRequestTask<PageResults<AnimeEpisodeInfo>> AnimeGetter::episodes_info(
    RequestorContext,
    GetFilters,
    std::optional<AnimeTrackID>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get episodes info");
}

NetworkRequestTask<PageResults<Comment>> AnimeGetter::comments(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get comments");
}

NetworkRequestTask<PageResults<RelatedWork>> AnimeGetter::related(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get related info");
}

NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> AnimeGetter::similar(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get similar info");
}

NetworkRequestTask<VideoSource> AnimeGetter::resolve_video(
    RequestorContext,
    VideoSource source) const {
	// Native players arrive already resolved; the default is an identity
	// passthrough. Embed-backed sources override to expand stream_url.
	co_return source;
}


NetworkRequestTask<SearchCompatibilities> AnimeRootGetter::search_support(RequestorContext) const {
	co_return SearchCompatibilities{};
}

AnimeGetterRootCompatibilities AnimeRootGetter::latest_support() const noexcept {
	return {};
}

std::vector<SearchQueryError> AnimeRootGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_support().supported_sorts, filters.sort);
}

NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> AnimeRootGetter::search(RequestorContext, SearchRequestQuery, GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search");
}

NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> AnimeRootGetter::latest(RequestorContext, GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot get latest");
}

NetworkRequestTask<std::vector<SearchSuggestion>> AnimeRootGetter::suggest(
    RequestorContext,
    std::string,
    std::optional<std::string>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot suggest search tokens");
}

NetworkRequestTask<std::unique_ptr<AnimeGetter>> AnimeRootGetter::parse_url(RequestorContext, ParsedUrl) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse url");
}

NetworkRequestTask<std::unique_ptr<AnimeGetter>> AnimeRootGetter::from_serialized(SerializedGetterData) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot deserialize data");
}

} // namespace aniparse
