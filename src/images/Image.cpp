/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/images/Image.hpp"
#include "aniparse/Exceptions.hpp"

namespace aniparse {

NetworkRequestTask<ImageContainerInfo> ImageContainerGetter::preview_info(RequestorContext context) {
	return info(std::move(context));
}

NetworkRequestTask<PageResults<Comment>> ImageContainerGetter::comments(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's ImageContainerGetter cannot get comments");
}

void ImageContainerGetter::reset() noexcept {
}

NetworkRequestTask<SearchCompatibilities> ImagesGetter::search_support(RequestorContext) {
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
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search images");
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> ImagesGetter::latest(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot get latest images");
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> ImagesGetter::parse_url(
    RequestorContext,
    ParsedUrl) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse image urls");
}

NetworkRequestTask<std::vector<SearchSuggestion>> ImagesGetter::suggest(
    RequestorContext,
    std::string,
    std::optional<std::string>) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot suggest search tokens");
}

} // namespace aniparse
