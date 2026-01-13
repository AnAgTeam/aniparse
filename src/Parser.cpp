#include "aniparse/Parser.hpp"
#include "aniparse/Exceptions.hpp"

namespace aniparse {
	std::unique_ptr<ImagesGetter> Parser::images_getter() const {
		return nullptr;
	}

	std::unique_ptr<MangaRootGetter> Parser::mangas_getter() const {
		return nullptr;
	}
}