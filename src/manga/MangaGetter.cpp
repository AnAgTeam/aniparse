/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/manga/MangaGetter.hpp"

namespace aniparse {
// A getter that was not handed a card knows nothing, and says so. It must not
// fall back to info(): a caller drawing a list would then pay a detail request
// per row without asking for one.
std::optional<MangaInfo> MangaGetter::preview_info() const noexcept {
	return std::nullopt;
}

NetworkRequestTask<PageResults<MangaTranslationInfo>> MangaGetter::load_translations(
    RequestContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get translation info");
}

NetworkRequestTask<PageResults<Comment>> MangaGetter::load_comments(
    RequestContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get comments");
}

NetworkRequestTask<PageResults<MangaChapterInfo>> MangaGetter::load_chapters(
    RequestContext,
    GetFilters,
    std::optional<MangaTranslationID>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get chapters info");
}

NetworkRequestTask<PageResults<RelatedWork>> MangaGetter::load_related(
    RequestContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get related info");
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaGetter::load_similar(
    RequestContext,
    GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get similar info");
}


} // namespace aniparse