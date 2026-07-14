/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/images/Image.hpp"

namespace aniparse {

// A getter that was not handed a card knows nothing, and says so. It must not
// fall back to info(): a caller drawing a list would then pay a detail request
// per row without asking for one.
std::optional<ImageContainerInfo> ImageContainerGetter::preview_info() const noexcept {
	return std::nullopt;
}

NetworkRequestTask<PageResults<Comment>> ImageContainerGetter::comments(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's ImageContainerGetter cannot get comments");
}


NetworkRequestTask<SearchCompatibilities> ImagesGetter::search_support(RequestorContext) const {
	co_return SearchCompatibilities{};
}

ImagesGetterRootCompatibilities ImagesGetter::latest_support() const noexcept {
	return {};
}

std::vector<SearchQueryError> ImagesGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_support().supported_sorts, filters.sort);
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> ImagesGetter::search(
    RequestorContext,
    SearchRequestQuery,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search images");
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> ImagesGetter::latest(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot get latest images");
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> ImagesGetter::parse_url(
    RequestorContext,
    ParsedUrl) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse image urls");
}

NetworkRequestTask<std::vector<SearchSuggestion>> ImagesGetter::suggest(
    RequestorContext,
    std::string,
    std::optional<std::string>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot suggest search tokens");
}

} // namespace aniparse
