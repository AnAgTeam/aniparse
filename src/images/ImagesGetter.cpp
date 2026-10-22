/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/images/ImagesGetter.hpp"

namespace aniparse {

// A getter that was not handed a card knows nothing, and says so. It must not
// fall back to info(): a caller drawing a list would then pay a detail request
// per row without asking for one.
std::optional<ImageContainerInfo> ImageContainerGetter::preview_info() const noexcept {
	return std::nullopt;
}

NetworkRequestTask<PageResults<Comment>> ImageContainerGetter::load_comments(
    RequestContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's ImageContainerGetter cannot get comments");
}


NetworkRequestTask<SearchCapabilities> ImagesGetter::load_search_capabilities(RequestContext) const {
	co_return SearchCapabilities{};
}

ImagesGetterCapabilities ImagesGetter::latest_capabilities() const noexcept {
	return {};
}

std::vector<SearchQueryError> ImagesGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_capabilities().supported_sorts, filters.sort);
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> ImagesGetter::search(
    RequestContext,
    SearchRequestQuery,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search images");
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> ImagesGetter::latest(
    RequestContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot get latest images");
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> ImagesGetter::parse_url(
    RequestContext,
    ParsedUrl) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse image urls");
}

NetworkRequestTask<std::vector<SearchSuggestion>> ImagesGetter::suggest(
    RequestContext,
    std::string,
    std::optional<std::string>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot suggest search tokens");
}

} // namespace aniparse
