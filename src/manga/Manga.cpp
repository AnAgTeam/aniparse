/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/manga/Manga.hpp"
#include "aniparse/Exceptions.hpp"

namespace aniparse {
std::vector<AltLink> resolve_alt_links(std::span<const std::string_view> builtin,
                                       const RequestorContext& context) {
	// The context overlays a live catalog override for its parser (if any) onto the
	// built-in list, so the returned list is the one actually fetched from.
	Mirrors mirrors = context.mirrors(builtin);
	std::vector<AltLink> resolved;
	resolved.reserve(mirrors.count());
	for (std::size_t index = 0; index < mirrors.count(); ++index) {
		resolved.push_back(AltLink{ std::string(mirrors.base_url(index)) });
	}
	return resolved;
}

NetworkRequestTask<MangaInfo> MangaGetter::preview_info(RequestorContext context) {
	return info(std::move(context));
}

NetworkRequestTask<PageResults<MangaTranslationInfo>> MangaGetter::translation_info(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get translation info");
}

NetworkRequestTask<PageResults<Comment>> MangaGetter::comments(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get comments");
}

NetworkRequestTask<PageResults<MangaChapterInfo>> MangaGetter::chapters_info(
    RequestorContext,
    GetFilters,
    std::optional<MangaTranslationID>) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get chapters info");
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaGetter::related(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get related info");
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaGetter::similar(
    RequestorContext,
    GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get similar info");
}

void MangaGetter::reset() noexcept {
}

NetworkRequestTask<SearchCompatibilities> MangaRootGetter::search_support(RequestorContext) {
	co_return SearchCompatibilities{};
}

MangaGetterRootCompatibilities MangaRootGetter::latest_support() const noexcept {
	return {};
}

std::vector<SearchQueryError> MangaRootGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_support().supported_sorts, filters.sort);
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::search(RequestorContext, SearchRequestQuery, GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search");
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::latest(RequestorContext, GetFilters) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot get latest");
}

NetworkRequestTask<std::vector<SearchSuggestion>> MangaRootGetter::suggest(
    RequestorContext,
    std::string,
    std::optional<std::string>) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot suggest search tokens");
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> MangaRootGetter::parse_url(RequestorContext, ParsedUrl) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse url");
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> MangaRootGetter::from_serialized(SerializedGetterData) {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot deserialize data");
}
} // namespace aniparse