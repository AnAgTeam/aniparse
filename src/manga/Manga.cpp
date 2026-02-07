/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/manga/Manga.hpp"
#include "aniparse/Exceptions.hpp"

namespace aniparse {
	NetworkRequestTask<MangaInfo> MangaGetter::preview_info(RequestorContext context) noexcept {
		return info(std::move(context));
	}

	NetworkRequestTask<std::vector<MangaTranslationInfo>> MangaGetter::translation_info(RequestorContext, GetFilters) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get translation info");
	}

	NetworkRequestTask<PageResults<MangaChapterInfo>> MangaGetter::chapters_info(
		RequestorContext,
		GetFilters,
		MangaTranslationID) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get chapters info");
	}

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaGetter::related(
		RequestorContext,
		GetFilters) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get related info");
	}

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaGetter::similar(
		RequestorContext, 
		GetFilters) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get similar info");
	}

	void MangaGetter::reset() noexcept {
		
	}

	SearchCompatibilities MangaRootGetter::search_support() const noexcept {
		return {};
	}

	MangaGetterRootCompatibilities MangaRootGetter::latest_support() const noexcept {
		return {};
	}

	NetworkRequestTask<RequestorContext> MangaRootGetter::authenticate_context(RequestorContext, AuthentificationData) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot auth");
	}

	RequestorContext MangaRootGetter::default_client_from(RequestorContext old_context) const noexcept {
		return old_context;
	}

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::search(RequestorContext, SearchRequestQuery, GetFilters) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search");
	}

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::latest(RequestorContext, GetFilters) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot get latest");
	}

	NetworkRequestTask<std::unique_ptr<MangaGetter>> MangaRootGetter::parse_url(RequestorContext, std::string) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse url");
	}

	NetworkRequestTask<std::unique_ptr<MangaGetter>> MangaRootGetter::from_serialized(SerializedGetterData) noexcept {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot deserialize data");
	}
}