#pragma once
#include "aniparse/DomainScanner.hpp"
#include "aniparse/Parser.hpp"

#include <string>
#include <map>
#include <numeric>
#include <optional>

namespace aniparse {

	class ParserStore {
	public:

		std::shared_ptr<Parser> add_parser(std::string_view domain, std::shared_ptr<Parser> parser);

		std::shared_ptr<Parser> find_by_domain(std::string_view domain);
		
		std::shared_ptr<Parser> find_for_url(std::string_view url);


	private:
		std::map<std::string, std::shared_ptr<Parser>> parsers_;
		DomainScanner<std::shared_ptr<Parser>> parsers_scanner_;
	};
}