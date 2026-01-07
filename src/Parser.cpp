#include "aniparse/Parser.hpp"

namespace aniparse {
	std::unique_ptr<ImagesGetter> Parser::images_getter() const {
		return nullptr;
	}
}