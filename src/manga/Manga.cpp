/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/manga/Manga.hpp"
#include "aniparse/Exceptions.hpp"

namespace aniparse {
	MangaGetter::MangaGetter(RequestorContext context) : context(std::move(context)) {

	}

	asyncnet::NetworkTask<MangaInfo> MangaGetter::preview_info() {
		return info();
	}

	asyncnet::NetworkTask<std::vector<MangaTranslationInfo>> MangaGetter::translation_info(GetFilters filters) {
		throw NotImplementedError("The parser's MangaGetter cannot get translation info");
	}

	asyncnet::NetworkTask<PageResults<MangaChapterInfo>> MangaGetter::chapters_info(
		GetFilters filters,
		MangaTranslationID translation) {
		throw NotImplementedError("The parser's MangaGetter cannot get chapters info");
	}

	asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> MangaGetter::related(GetFilters filters) {
		throw NotImplementedError("The parser's MangaGetter cannot get related info");
	}

	asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> MangaGetter::similar(GetFilters filters) {
		throw NotImplementedError("The parser's MangaGetter cannot get similar info");
	}

	void MangaGetter::reset() {
		throw NotImplementedError("The parser's MangaGetter cannot be resetted");
	}

	bool MangaGetter::update_context(RequestorContext context) {
		this->context = std::move(context);
		return true;
	}

	SearchCompatibilities MangaRootGetter::search_support() const {
		return {};
	}

	MangaGetterRootCompatibilities MangaRootGetter::latest_support() const {
		return {};
	}

	asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::search(RequestorContext context, SearchRequestQuery query, GetFilters filters) {
		throw NotImplementedError("The parser cannot search");
	}

	asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::latest(RequestorContext context, GetFilters filters) {
		throw NotImplementedError("The parser cannot get latest");
	}

	asyncnet::NetworkTask<std::unique_ptr<MangaGetter>> MangaRootGetter::parse_url(RequestorContext context, std::string url) {
		throw NotImplementedError("The parser cannot parse url");
	}
	asyncnet::NetworkTask<std::unique_ptr<MangaGetter>> MangaRootGetter::from_serialized(SerializedGetterData data) {
		throw NotImplementedError("The parser cannot deserialize data");
	}
}