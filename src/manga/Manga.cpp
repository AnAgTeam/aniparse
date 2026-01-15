#include "aniparse/manga/Manga.hpp"
#include "aniparse/Exceptions.hpp"

namespace aniparse {
	MangaGetter::MangaGetter(RequestorContext context) : context(std::move(context)) {

	}

	asyncnet::NetworkTask<std::vector<MangaTranslationInfo>> MangaGetter::translation_info(GetFilters filters) {
		throw NotImplementedError("The parser's MangaGetter cannot get translation info");
	}

	asyncnet::NetworkTask<std::vector<MangaChapterInfo>> MangaGetter::chapters_info(GetFilters filters) {
		throw NotImplementedError("The parser's MangaGetter cannot get chapters info");
	}

	asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> MangaGetter::similar() {
		throw NotImplementedError("The parser's MangaGetter cannot get similar info");
	}

	bool MangaGetter::update_context(RequestorContext context) {
		this->context = std::move(context);
		return true;
	}

	asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> MangaRootGetter::search(RequestorContext context, std::string query, GetFilters filters) {
		throw NotImplementedError("The parser cannot search");
	}

	asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> MangaRootGetter::latest(RequestorContext context, GetFilters filters) {
		throw NotImplementedError("The parser cannot get latest");
	}

	asyncnet::NetworkTask<std::unique_ptr<MangaGetter>> MangaRootGetter::parse_url(RequestorContext context, std::string_view url) {
		throw NotImplementedError("The parser cannot parse url");
	}
}