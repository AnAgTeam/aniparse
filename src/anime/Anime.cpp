/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/anime/Anime.hpp"

namespace aniparse {

NetworkRequestTask<AnimeInfo> AnimeGetter::preview_info(RequestorContext context) {
	return info(std::move(context));
}

NetworkRequestTask<PageResults<AnimeTrackInfo>> AnimeGetter::tracks(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot list tracks");
}

NetworkRequestTask<PageResults<AnimeEpisodeInfo>> AnimeGetter::episodes_info(
    RequestorContext,
    GetFilters,
    std::optional<AnimeTrackID>) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get episodes info");
}

NetworkRequestTask<PageResults<Comment>> AnimeGetter::comments(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get comments");
}

NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> AnimeGetter::related(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get related info");
}

NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> AnimeGetter::similar(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's AnimeGetter cannot get similar info");
}

NetworkRequestTask<VideoSource> AnimeGetter::resolve_video(
    RequestorContext,
    VideoSource source) {
	// Native players arrive already resolved; the default is an identity
	// passthrough. Embed-backed sources override to expand stream_url.
	co_return source;
}


NetworkRequestTask<SearchCompatibilities> AnimeRootGetter::search_support(RequestorContext) {
	co_return SearchCompatibilities{};
}

AnimeGetterRootCompatibilities AnimeRootGetter::latest_support() const noexcept {
	return {};
}

std::vector<SearchQueryError> AnimeRootGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_support().supported_sorts, filters.sort);
}

NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> AnimeRootGetter::search(RequestorContext, SearchRequestQuery, GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search");
}

NetworkRequestTask<PageResults<std::unique_ptr<AnimeGetter>>> AnimeRootGetter::latest(RequestorContext, GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot get latest");
}

NetworkRequestTask<std::vector<SearchSuggestion>> AnimeRootGetter::suggest(
    RequestorContext,
    std::string,
    std::optional<std::string>) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot suggest search tokens");
}

NetworkRequestTask<std::unique_ptr<AnimeGetter>> AnimeRootGetter::parse_url(RequestorContext, ParsedUrl) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse url");
}

NetworkRequestTask<std::unique_ptr<AnimeGetter>> AnimeRootGetter::from_serialized(SerializedGetterData) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot deserialize data");
}

} // namespace aniparse
