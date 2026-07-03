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

NetworkRequestTask<PageResults<MangaTranslationInfo>> MangaGetter::translation_info(
    RequestorContext,
    GetFilters) noexcept {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The parser's MangaGetter cannot get translation info");
}

NetworkRequestTask<PageResults<MangaChapterInfo>> MangaGetter::chapters_info(
    RequestorContext,
    GetFilters,
    std::optional<MangaTranslationID>) noexcept {
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

std::shared_ptr<ParserConfig> MangaRootGetter::default_config_from(std::shared_ptr<const ParserConfig> base_config) const {
	if (!base_config)
		return nullptr;
	auto new_config = std::make_shared<ParserConfig>(*base_config);
	// Do not inherit the base config's cookie jar: a derived config belongs to a
	// distinct parser instance and must get its own store (lazily provisioned by
	// RequestorContext), otherwise two instances of the same parser would share a
	// session and their logins would collide. Restoring a saved session is a
	// separate, explicit path (assign a deserialized jar after deriving).
	new_config->cookie_jar = nullptr;
	return new_config;
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
} // namespace aniparse