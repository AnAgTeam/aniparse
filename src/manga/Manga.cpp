#include "aniparse/manga/Manga.hpp"
#include "aniparse/Exceptions.hpp"

namespace aniparse {
	asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> MangaRootGetter::search(std::shared_ptr<ClientContext> client, std::string query, GetFilters filters) {
		throw NotImplementedError("The parser cannot search");
	}

	asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> MangaRootGetter::latest(std::shared_ptr<ClientContext> client, GetFilters filters) {
		throw NotImplementedError("The parser cannot get latest");
	}

	asyncnet::NetworkTask<std::unique_ptr<MangaGetter>> MangaRootGetter::parse_url(std::shared_ptr<ClientContext> client, std::string_view url) {
		throw NotImplementedError("The parser cannot parse url");
	}
}