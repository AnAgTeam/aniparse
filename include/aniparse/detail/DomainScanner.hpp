/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aniparse {
template <typename DomainParser, typename Char = char>
class DomainScanner {
	using DomainStringView = std::basic_string_view<Char>;
	using DomainString     = std::basic_string<Char>;

	struct Node {
		using Children = std::map<DomainString, Node, std::less<>>;

		explicit Node(Node* parent = nullptr) : parent(parent) {
		}

		Node* find(DomainStringView name) {
			auto iter = children.find(name);
			return iter == children.end() ? nullptr : &iter->second;
		}

		Node& child(DomainStringView name) {
			auto result = children.try_emplace(DomainString{name}, this);
			return result.first->second;
		}

		Node*                       parent;
		std::vector<DomainParser>   parsers;
		Children                    children;
	};

public:
	DomainScanner()                                = default;
	DomainScanner(const DomainScanner&)            = delete;
	DomainScanner& operator=(const DomainScanner&) = delete;
	DomainScanner(DomainScanner&&)                 = delete;
	DomainScanner& operator=(DomainScanner&&)      = delete;

	template <typename T>
	void add_domain_parser(std::initializer_list<T> domains, DomainParser parser) {
		add_domain_parser_impl(domains.begin(), domains.end(), std::move(parser));
	}

	template <typename Range>
	void add_domain_parser(Range&& domains, DomainParser parser) {
		add_domain_parser_impl(std::ranges::begin(domains), std::ranges::end(domains), std::move(parser));
	}

	template <typename Iterator, typename Sentinel>
	void add_domain_parser(Iterator first, Sentinel last, DomainParser parser) {
		add_domain_parser_impl(first, last, std::move(parser));
	}

	template <typename Range, typename... Args>
	void emplace_domain_parser(Range&& domains, Args&&... args) {
		emplace_domain_parser_impl(
		    std::ranges::begin(domains), std::ranges::end(domains), std::forward<Args>(args)...);
	}

	template <typename Iterator, typename Sentinel, typename... Args>
	void emplace_domain_parser(Iterator first, Sentinel last, Args&&... args) {
		emplace_domain_parser_impl(first, last, std::forward<Args>(args)...);
	}

	template <typename Range>
	void remove_domain_parser(Range&& domains, const DomainParser& parser) {
		remove_domain_parser_impl(std::ranges::begin(domains), std::ranges::end(domains), parser);
	}

	template <typename Iterator, typename Sentinel>
	void remove_domain_parser(Iterator first, Sentinel last, const DomainParser& parser) {
		remove_domain_parser_impl(first, last, parser);
	}

	template <typename T, typename Predicate>
	std::optional<DomainParser> search(std::initializer_list<T> domains, Predicate pred) {
		return search_impl(domains.begin(), domains.end(), std::move(pred));
	}

	template <typename Range, typename Predicate>
	std::optional<DomainParser> search(Range&& domains, Predicate pred) {
		return search_impl(std::ranges::begin(domains), std::ranges::end(domains), std::move(pred));
	}

	template <typename Iterator, typename Sentinel, typename Predicate>
	std::optional<DomainParser> search(Iterator first, Sentinel last, Predicate pred) {
		return search_impl(first, last, std::move(pred));
	}

	template <typename T, typename Predicate>
	std::optional<DomainParser> search_all(std::initializer_list<T> domains, Predicate pred) {
		return search_all_impl(domains.begin(), domains.end(), std::move(pred));
	}

	template <typename Range, typename Predicate>
	std::optional<DomainParser> search_all(Range&& domains, Predicate pred) {
		return search_all_impl(std::ranges::begin(domains), std::ranges::end(domains), std::move(pred));
	}

	template <typename Iterator, typename Sentinel, typename Predicate>
	std::optional<DomainParser> search_all(Iterator first, Sentinel last, Predicate pred) {
		return search_all_impl(first, last, std::move(pred));
	}

private:
	static DomainString to_domain_string(DomainStringView part) {
		return DomainString{part};
	}

	template <typename Range>
	static DomainString to_domain_string(const Range& part) {
		return DomainString{std::ranges::begin(part), std::ranges::end(part)};
	}

	template <typename Predicate>
	static std::optional<DomainParser> find_parser_by_pred(Node& node, Predicate& pred) {
		auto iter = std::find_if(node.parsers.begin(), node.parsers.end(), [&](DomainParser& parser) {
			return std::invoke(pred, parser);
		});
		return iter == node.parsers.end() ? std::nullopt : std::optional<DomainParser>{*iter};
	}

	template <typename Iterator, typename Sentinel>
	Node& find_or_create_node(Iterator first, Sentinel last) {
		Node* node = &root_;
		while (first != last) {
			node = std::addressof(node->child(to_domain_string(*first++)));
		}
		return *node;
	}

	template <typename Iterator, typename Sentinel>
	Node* find_node(Iterator first, Sentinel last) {
		Node* node = &root_;
		while (first != last) {
			node = node->find(to_domain_string(*first++));
			if (!node) {
				return nullptr;
			}
		}
		return node;
	}

	template <typename Iterator, typename Sentinel>
	Node* find_deepest_node(Iterator first, Sentinel last) {
		Node* node = &root_;
		while (first != last) {
			Node* next = node->find(to_domain_string(*first++));
			if (!next) {
				break;
			}
			node = next;
		}
		return node;
	}

	template <typename Iterator, typename Sentinel>
	void add_domain_parser_impl(Iterator first, Sentinel last, DomainParser parser) {
		find_or_create_node(first, last).parsers.emplace_back(std::move(parser));
	}

	template <typename Iterator, typename Sentinel, typename... Args>
	void emplace_domain_parser_impl(Iterator first, Sentinel last, Args&&... args) {
		find_or_create_node(first, last).parsers.emplace_back(std::forward<Args>(args)...);
	}

	template <typename Iterator, typename Sentinel>
	void remove_domain_parser_impl(Iterator first, Sentinel last, const DomainParser& parser) {
		if (Node* node = find_node(first, last)) {
			std::erase(node->parsers, parser);
		}
	}

	template <typename Iterator, typename Sentinel, typename Predicate>
	std::optional<DomainParser> search_impl(Iterator first, Sentinel last, Predicate pred) {
		Node* node = find_node(first, last);
		return node ? find_parser_by_pred(*node, pred) : std::nullopt;
	}

	template <typename Iterator, typename Sentinel, typename Predicate>
	std::optional<DomainParser> search_all_impl(Iterator first, Sentinel last, Predicate pred) {
		for (Node* node = find_deepest_node(first, last); node; node = node->parent) {
			if (auto parser = find_parser_by_pred(*node, pred)) {
				return parser;
			}
		}
		return std::nullopt;
	}

	Node root_;
};
} // namespace aniparse
