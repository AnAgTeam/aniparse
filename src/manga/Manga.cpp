/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/manga/Manga.hpp"

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

// A getter that was not handed a card knows nothing, and says so. It must not
// fall back to info(): a caller drawing a list would then pay a detail request
// per row without asking for one.
std::optional<MangaInfo> MangaGetter::preview_info() const noexcept {
	return std::nullopt;
}

NetworkRequestTask<PageResults<MangaTranslationInfo>> MangaGetter::translation_info(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get translation info");
}

NetworkRequestTask<PageResults<Comment>> MangaGetter::comments(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get comments");
}

NetworkRequestTask<PageResults<MangaChapterInfo>> MangaGetter::chapters_info(
    RequestorContext,
    GetFilters,
    std::optional<MangaTranslationID>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get chapters info");
}

NetworkRequestTask<PageResults<RelatedWork>> MangaGetter::related(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get related info");
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaGetter::similar(
    RequestorContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get similar info");
}


NetworkRequestTask<SearchCompatibilities> MangaRootGetter::search_support(RequestorContext) const {
	co_return SearchCompatibilities{};
}

MangaGetterRootCompatibilities MangaRootGetter::latest_support() const noexcept {
	return {};
}

std::vector<SearchQueryError> MangaRootGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_support().supported_sorts, filters.sort);
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::search(RequestorContext, SearchRequestQuery, GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search");
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::latest(RequestorContext, GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot get latest");
}

NetworkRequestTask<std::vector<SearchSuggestion>> MangaRootGetter::suggest(
    RequestorContext,
    std::string,
    std::optional<std::string>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot suggest search tokens");
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> MangaRootGetter::parse_url(RequestorContext, ParsedUrl) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse url");
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> MangaRootGetter::from_serialized(SerializedGetterData) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot deserialize data");
}
} // namespace aniparse