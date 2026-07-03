/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/DomainScanner.hpp"
#include "aniparse/Parser.hpp"

#include <string>
#include <map>
#include <numeric>
#include <optional>

namespace aniparse {

/**
 * @brief Class for storing and retrieving parsers
 */
class ParserStore {
public:
	ParserStore();

	/**
	 * @brief Add parser to the store
	 * @see Parser
	 * Calls parser->emplace_domains(...), when the parser have to add its domains.
	 * Later the parser can be got from @ref find_by_domain, @ref find_for_url, @ref find_by_key
	 * @param parser The parser to add
	 * @throw std::logic_error If the parser identifier is conflicting with existing parsers
	 * @return Stored parser
	 */
	std::shared_ptr<Parser> add_parser(std::shared_ptr<Parser> parser);

	/**
	 * @brief Search parser suitable for the domain
	 * @see Parser
	 * @param domain The domain to search parser for
	 * @return Parser if found, empty pointer otherwise
	 */
	std::shared_ptr<Parser> find_by_domain(std::string_view domain);

	/**
	 * @brief Search parser suitable for the url
	 * @see Parser
	 * @param url The url to search parser for
	 * @return Parser if found, empty pointer otherwise
	 */
	std::shared_ptr<Parser> find_for_url(std::string_view url);

	/**
	 * @brief Search parser by it's unique identifier
	 * @see Parser
	 * @param key Unique parser identifier
	 * @return Parser if found, empty pointer otherwise
	 */
	std::shared_ptr<Parser> find_by_key(std::string_view key);

private:
	/**
	 * Check if the parser identifier doesn't conflicting with existing parsers
	 * @see Parser
	 * @param parser The parser to check identifier uniqueness
	 * @return true if the identifier is unique, false otherwise
	 */
	bool check_is_conflicting(const std::shared_ptr<Parser>& parser) const;

	std::map<std::string, std::shared_ptr<Parser>, std::less<>> parsers_;
	DomainScanner<std::shared_ptr<Parser>> parsers_scanner_;
};
} // namespace aniparse