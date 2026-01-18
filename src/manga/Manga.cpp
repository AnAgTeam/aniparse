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

	asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> MangaGetter::similar() {
		throw NotImplementedError("The parser's MangaGetter cannot get similar info");
	}

	void MangaGetter::reset() {
		throw NotImplementedError("The parser's MangaGetter cannot be resetted");
	}

	bool MangaGetter::update_context(RequestorContext context) {
		this->context = std::move(context);
		return true;
	}

	MangaGetterRootCompatibilities MangaRootGetter::search_support() const {
		return {};
	}

	MangaGetterRootCompatibilities MangaRootGetter::latest_support() const {
		return {};
	}

	asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::search(RequestorContext context, std::string query, GetFilters filters) {
		throw NotImplementedError("The parser cannot search");
	}

	asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> MangaRootGetter::latest(RequestorContext context, GetFilters filters) {
		throw NotImplementedError("The parser cannot get latest");
	}

	asyncnet::NetworkTask<std::unique_ptr<MangaGetter>> MangaRootGetter::parse_url(RequestorContext context, std::string url) {
		throw NotImplementedError("The parser cannot parse url");
	}
}