/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/ParserStore.hpp"
#include "aniparse/types/ParsedUrl.hpp"

#include <stdexcept>

namespace aniparse {
std::shared_ptr<Parser> ParserStore::add_parser(std::shared_ptr<Parser> parser) {
	auto edit = begin_edit();
	auto added = edit.add_parser(std::move(parser));
	edit.commit();
	return added;
}

std::shared_ptr<Parser> ParserStore::Edit::add_parser(std::shared_ptr<Parser> parser) {
	std::string identifier = parser->identifier();
	if (edit_.contains(identifier)) {
		throw std::logic_error("Conflicting parser name: " + identifier);
	}

	return edit_.add(std::move(identifier), std::move(parser));
}

bool ParserStore::Edit::remove_parser(std::string_view identifier) {
	return edit_.remove(identifier);
}

void ParserStore::Edit::refresh_domains(
    std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains) {
	edit_.set_volatile_domains(std::move(volatile_domains));
}

void ParserStore::Edit::commit() {
	edit_.commit();
}

void ParserStore::refresh_domains(std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains) {
	auto edit = begin_edit();
	edit.refresh_domains(std::move(volatile_domains));
	edit.commit();
}

std::shared_ptr<Parser> ParserStore::find_by_domain(std::string_view domain) {
	std::string url = "https://" + std::string(domain);
	auto parsed     = ParsedUrl::parse(url);
	if (!parsed) {
		return nullptr;
	}
	return domains_.find_by_host(parsed->host(), [&parsed](Parser& parser) {
		return parser.valid_for_url(*parsed);
	});
}

std::shared_ptr<Parser> ParserStore::find_for_url(std::string_view url) {
	auto parsed = ParsedUrl::parse(url);
	if (!parsed) {
		return nullptr;
	}
	return domains_.find_by_host(parsed->host(), [&parsed](Parser& parser) {
		return parser.valid_for_url(*parsed);
	});
}

std::optional<UrlRoute> ParserStore::route_url(std::string_view url) {
	auto parsed = ParsedUrl::parse(url);
	if (!parsed) {
		return std::nullopt;
	}
	auto found_parser = domains_.find_by_host(parsed->host(), [&parsed](Parser& parser) {
		return parser.valid_for_url(*parsed);
	});
	if (!found_parser) {
		return std::nullopt;
	}
	GetterSuggestionType type = found_parser->suggest_getter(*parsed);
	return UrlRoute{ std::move(found_parser), type, std::move(*parsed) };
}

std::shared_ptr<Parser> ParserStore::find_by_key(std::string_view key) {
	return domains_.find_by_key(key);
}

std::vector<std::shared_ptr<Parser>> ParserStore::parsers() const {
	return domains_.values();
}

} // namespace aniparse
