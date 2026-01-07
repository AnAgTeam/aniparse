#pragma once
#include "aniparse/DomainScanner.hpp"
#include "aniparse/Parser.hpp"

#include <string>
#include <map>
#include <numeric>
#include <optional>
#include <ranges>

namespace aniparse {

	namespace detail {
		template<typename Iter>
		size_t domain_iter_size(Iter first, Iter last) {
			size_t total_length = std::accumulate(first, last, 0, [](size_t current_sum, const auto& value) {
				return current_sum + value.size();
			});
			return total_length + 1; // '0'
		}

		template<typename Iter>
		std::string join_domains(Iter first, Iter last) {
			if (first == last) {
				return "";
			}

			std::string joined;
			joined.reserve(domain_iter_size(first, last));
			joined += *first;

			std::accumulate(std::next(first), last, joined, [](std::string& str, auto&& value) {
				str += '.';
				return str += value;
			});
			return joined;
		}
	}

	template<typename T>
	class DomainStore {
	public:
		DomainStore() = default;

		void insert(std::string domain, T value) {
			storage_.emplace(std::move(domain), std::move(value));
		}

		void insert(std::initializer_list<std::string_view> domain_tree, T value) {
			auto joined_domain = detail::join_domains(domain_tree.begin(), domain_tree.end());
			storage_.emplace(std::move(joined_domain), std::move(value));
		}

		template<typename Predicate>
		const T* find(std::string domain, Predicate pred) {
			while (domain) {
				if (const T* found = find_node(domain, pred)) {
					return found;
				}
				domain = domain.substr(domain.find('.'));
			}
		}

	private:

		template<typename Predicate>
		const T* find_node(const std::string& domain, const Predicate& pred) {
			auto storage_iter = storage_.find(domain);
			auto found_iter = std::find_if(std::begin(storage_iter), std::end(storage_iter), pred);
			if (found_iter == std::end(storage_iter)) {
				return nullptr;
			}
			return std::addressof(*found_iter);
		}

		std::multimap<std::string, T> storage_;
	};

	class ParserStore {
	public:

		inline Parser* add_parser(std::string_view domain, std::unique_ptr<Parser> parser) {
			auto domain_iter = domain | std::views::split('.');
			auto& inserted_parser = parsers_[parser->identifier()] = std::move(parser);
			parsers_scanner_.add_domain_parser(domain_iter, inserted_parser.get());
		}

		//inline Parser* find_by_domain(std::string_view domain) {
		//	auto domain_iter = domain | std::views::split('.');
		//	std::string url = "https://" + std::string(domain);
		//	auto found_parser = parsers_scanner_.search_all(domain_iter, [&url](Parser* parser) {
		//		return parser->valid_for_url(url);
		//	});
		//	return found_parser ? *found_parser : nullptr;
		//}

		//inline Parser* find_for_url(std::string_view url) {
		//	auto domain_iter = url
		//		| std::views::drop_while([](char c) { return c != ':'; })
		//		| std::views::take_while([](char c) { return c != '?'; })
		//		| std::views::split('.');
		//	auto found_parser = parsers_scanner_.search_all(std::begin(domain_iter), std::end(domain_iter), [&url](const Parser* parser) {
		//		return parser->valid_for_url(url);
		//	});
		//	return found_parser ? *found_parser : nullptr;
		//}

		//template<typename Iter>
		//void add_parser(Iter domain_first, Iter domain_last, Parser parser) {
		//	add_parser(domain_first, domain_last, std::make_unique<Parser>(std::move(parser)))
		//}

	private:
		std::map<std::string, std::unique_ptr<Parser>> parsers_;
		DomainScanner<Parser*> parsers_scanner_;
	};
}