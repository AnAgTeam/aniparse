/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/ParserStore.hpp"
#include "aniparse/types/ParsedUrl.hpp"

#include <ranges>

namespace aniparse {
namespace detail {
template <std::ranges::viewable_range Range>
static constexpr auto split_domains(Range&& range) {
	// clang-format off
	return std::forward<Range>(range)
	       // split domain in reverse order
	       | std::views::reverse
	       | std::views::split('.')
	       | std::views::transform(std::views::reverse);
	// clang-format on
}

static constexpr auto split_url_domains(std::string_view url) {
	size_t protocol_end = url.find("://");
	size_t drop_start   = protocol_end == std::string_view::npos ? 0 : protocol_end + 3;
	// clang-format off
	return url
	       // cut domain from url
	       | std::views::drop(drop_start)
	       | std::views::take_while([](char c) { return c != '/' && c != '\\' && c != /*params*/ '?' && c != /*id*/ '#' && c != /*port*/ ':'; })
	       // split domain in reverse order
	       | std::views::reverse
	       | std::views::split('.')
	       | std::views::transform(std::views::reverse);
	// clang-format on
}

class DomainAdder : public aniparse::EmplaceDomainsContext {
public:
	DomainAdder(DomainScanner<std::shared_ptr<Parser>>& scanner, std::shared_ptr<Parser> parser)
	    : scanner_(scanner), parser_(std::move(parser)) {
	}

	void add_domain(std::string_view domain) override {
		auto domain_iter = detail::split_domains(domain);
		scanner_.add_domain_parser(domain_iter, parser_);
	}

private:
	DomainScanner<std::shared_ptr<Parser>>& scanner_;
	std::shared_ptr<Parser> parser_;
};
} // namespace detail

ParserStore::ParserStore() {
	rebuild_index();
}

std::shared_ptr<Parser> ParserStore::add_parser(std::shared_ptr<Parser> parser) {
	if (check_is_conflicting(parser)) {
		throw std::logic_error("Conflicting parser name: " + parser->identifier());
	}

	auto& inserted_parser = parsers_[parser->identifier()] = std::move(parser);
	rebuild_index();
	return inserted_parser;
}

std::shared_ptr<ParserStore::Scanner> ParserStore::build_scanner() const {
	auto scanner = std::make_shared<Scanner>();
	for (const auto& [key, parser] : parsers_) {
		detail::DomainAdder adder(*scanner, parser);
		parser->emplace_domains(adder);
		if (auto volatile_iter = volatile_domains_.find(key); volatile_iter != volatile_domains_.end()) {
			for (const std::string& domain : volatile_iter->second) {
				adder.add_domain(domain);
			}
		}
	}
	return scanner;
}

void ParserStore::rebuild_index() {
	scanner_.store(build_scanner());
}

void ParserStore::refresh_domains(std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains) {
	volatile_domains_ = std::move(volatile_domains);
	rebuild_index();
}

std::shared_ptr<Parser> ParserStore::find_by_domain(std::string_view domain) {
	std::string url = "https://" + std::string(domain);
	auto parsed     = ParsedUrl::parse(url);
	if (!parsed) {
		return nullptr;
	}
	auto scanner      = scanner_.load();
	auto domain_iter  = detail::split_domains(domain);
	auto found_parser = scanner->search_all(domain_iter, [&parsed](std::shared_ptr<Parser>& parser) {
		return parser->valid_for_url(*parsed);
	});
	return found_parser ? *found_parser : nullptr;
}

std::shared_ptr<Parser> ParserStore::find_for_url(std::string_view url) {
	auto parsed = ParsedUrl::parse(url);
	if (!parsed) {
		return nullptr;
	}
	auto scanner      = scanner_.load();
	auto domain_iter  = detail::split_url_domains(url);
	auto found_parser = scanner->search_all(domain_iter, [&parsed](std::shared_ptr<Parser>& parser) {
		return parser->valid_for_url(*parsed);
	});
	return found_parser ? *found_parser : nullptr;
}

std::optional<UrlRoute> ParserStore::route_url(std::string_view url) {
	auto parsed = ParsedUrl::parse(url);
	if (!parsed) {
		return std::nullopt;
	}
	auto scanner      = scanner_.load();
	auto domain_iter  = detail::split_url_domains(url);
	auto found_parser = scanner->search_all(domain_iter, [&parsed](std::shared_ptr<Parser>& parser) {
		return parser->valid_for_url(*parsed);
	});
	if (!found_parser) {
		return std::nullopt;
	}
	GetterSuggestionType type = (*found_parser)->suggest_getter(*parsed);
	return UrlRoute{ *found_parser, type, std::move(*parsed) };
}

std::shared_ptr<Parser> ParserStore::find_by_key(std::string_view key) {
	auto iter = parsers_.find(key);
	return iter != parsers_.end() ? iter->second : nullptr;
}

std::vector<std::shared_ptr<Parser>> ParserStore::parsers() const {
	std::vector<std::shared_ptr<Parser>> result;
	result.reserve(parsers_.size());
	for (const auto& [key, parser] : parsers_) {
		result.push_back(parser);
	}
	return result;
}

bool ParserStore::check_is_conflicting(const std::shared_ptr<Parser>& parser) const {
	auto iter = parsers_.find(parser->identifier());
	return iter != parsers_.end();
}
} // namespace aniparse