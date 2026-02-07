/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <string_view>
#include <string>
#include <optional>
#include <memory>
#include <map>
#include <vector>
#include <ranges>

namespace aniparse {
	template<
		typename DomainParser,
		typename Char = char
	>
	class DomainNode : public std::enable_shared_from_this<DomainNode<DomainParser, Char>> {
		struct PrivateConstructor {
			explicit constexpr PrivateConstructor() = default;
		};

	public:
		using DomainStringView = std::basic_string_view<Char>;
		using DomainString = std::basic_string<Char>;

		using Node = std::shared_ptr<DomainNode>;
		using NodesContainer = std::map<DomainString, Node, std::less<>>;
		using ParserContainer = std::vector<DomainParser>;

		static constexpr Node null_node;

		DomainNode(PrivateConstructor) {}

		DomainNode(Node& parent_node, PrivateConstructor) : parent_(parent_node) {

		}

		DomainNode(const DomainNode& other) = delete;
		DomainNode(DomainNode&& other) = default;
		~DomainNode() = default;

		DomainNode& operator=(const DomainNode& other) = delete;
		DomainNode& operator=(DomainNode&& other) = default;

		static Node make_node(Node parent = null_node) {
			return std::make_shared<DomainNode>(parent, PrivateConstructor{});
		}

		template<std::ranges::range Range>
			requires requires (Range range) {
				DomainString(std::begin(range), std::end(range));
			}
		Node find(Range range) {
			DomainStringView str(std::begin(range), std::end(range));
			auto iter = nexts_.find(str);
			return iter != nexts_.end() ? iter->second : null_node;
		}

		template<std::three_way_comparable_with<DomainString> T>
		Node find(T name) {
			auto iter = nexts_.find(name);
			return iter != nexts_.end() ? iter->second : null_node;
		}

		template<typename T>
		Node& operator[](T&& name) {
			Node node = find(name);
			return node != null_node ? node : insert(std::forward<T>(name));
		}

		template<std::convertible_to<DomainString> T>
		Node& insert(T name) {
			Node& new_node = nexts_[std::move(name)] = make_node(this->shared_from_this());
			return new_node;
		}

		template<std::ranges::range Range>
			requires requires (Range range) {
				DomainString(std::begin(range), std::end(range));
			}
		Node& insert(Range range) {
			DomainString str(std::begin(range), std::end(range));
			Node& new_node = nexts_[std::move(str)] = make_node(this->shared_from_this());
			return new_node;
		}

		ParserContainer& parsers() {
			return parsers_;
		}

		template<typename ... Args>
			requires std::constructible_from<DomainParser, Args&& ...>
		DomainParser& emplace_parser(Args&& ... args) {
			parsers_.emplace(std::forward<Args>(args) ...);
		}

		DomainParser& insert_parser(const DomainParser& parser) {
			parsers_.emplace(parser);
		}

		DomainParser& insert_parser(DomainParser&& parser) {
			parsers_.push_back(std::move(parser));
			return *std::rbegin(parsers_);
		}

		void erase(const DomainParser& parser) {
			std::erase(parsers_.begin(), parsers_.end(), parser);
		}

		Node parent() const {
			return parent_.lock();
		}

	private:

		std::weak_ptr<DomainNode> parent_;
		ParserContainer parsers_;
		NodesContainer nexts_;
	};

	template<
		typename DomainParser,
		typename Char = char
	>
	class DomainStorage {
	public:

		using Node = std::shared_ptr<DomainNode<DomainParser, Char>>;

		Node& first() {
			return domains_start_;
		}

		Node& last() {
			return DomainNode<DomainParser, Char>::null_node;
		}

	private:

		Node domains_start_ = DomainNode<DomainParser, Char>::make_node();
	};

	template<typename DomainParser, typename Char = char, typename Storage = DomainStorage<DomainParser, Char>>
	class DomainScanner {
		using Node = typename Storage::Node;

	public:

		template<typename T>
		void add_domain_parser(std::initializer_list<T> range, DomainParser parser) {
			add_domain_parser(std::begin(range), std::end(range), std::move(parser));
		}

		template<std::ranges::range Range>
		void add_domain_parser(Range range, DomainParser parser) {
			add_domain_parser(std::begin(range), std::end(range), std::move(parser));
		}

		template<typename Iterator>
		void add_domain_parser(Iterator first, Iterator last, DomainParser parser) {
			find_node(first, last).insert_parser(std::move(parser));
		}

		template<std::ranges::range Range, typename ... Args>
			requires std::constructible_from<DomainParser, Args&&...>
		void emplace_domain_parser(const Range& range, Args&& ... args) {
			emplace_domain_parser(std::begin(range), std::end(range), std::forward<Args>(args) ...);
		}

		template<typename Iterator, typename ... Args>
			requires std::constructible_from<DomainParser, Args&&...>
		void emplace_domain_parser(Iterator first, Iterator last, Args&& ... args) {
			find_node(first, last).emplace(std::forward<Args>(args) ...);
		}

		template<std::ranges::range Range>
		void remove_domain_parser(const Range& range, const DomainParser& parser) {
			remove_domain_parser(std::begin(range), std::end(range), parser);
		}

		template<typename Iterator>
		void remove_domain_parser(Iterator first, Iterator last, const DomainParser& parser) {
			find_node(first, last).erase(parser);
		}

		void remove_parsers(const DomainParser& parser) = delete;

		template<typename T, typename Predicate>
		std::optional<DomainParser> search(std::initializer_list<T> range, Predicate pred) {
			return search(std::begin(range), std::end(range), pred);
		}

		template<std::ranges::range Range, typename Predicate>
		std::optional<DomainParser> search(Range range, Predicate pred) {
			return search(std::begin(range), std::end(range), pred);
		}

		template<typename Iterator, typename Predicate>
		std::optional<DomainParser> search(Iterator first, Iterator last, Predicate pred) {
			return find_parser_by_pred(find_node(first, last), pred);
		}

		template<typename T, typename Predicate>
		std::optional<DomainParser> search_all(std::initializer_list<T> range, Predicate pred) {
			return search_all(std::begin(range), std::end(range), pred);
		}

		template<std::ranges::range Range, typename Predicate>
		std::optional<DomainParser> search_all(Range range, Predicate pred) {
			return search_all(std::begin(range), std::end(range), pred);
		}

		template<std::forward_iterator Iterator, typename Predicate>
		std::optional<DomainParser> search_all(Iterator first, Iterator last, Predicate pred) {
			auto node = std::addressof(find_node_no_add(first, last));
			if (auto parser = find_parser_by_pred(*node, pred)) {
				return parser;
			}

			while ((node = get_node_ptr(node->parent()))) {
				if (auto parser = find_parser_by_pred(*node, pred)) {
					return parser;
				}
			}
			return std::nullopt;
		}

	private:

		static constexpr bool node_is_ptr = requires (Node node) { *node; };

		template<typename Predicate>
		std::optional<DomainParser> find_parser_by_pred(auto& node, Predicate pred) {
			auto& parsers = node.parsers();
			auto iter = std::find_if(std::begin(parsers), std::end(parsers), pred);
			if (iter == parsers.end()) return std::nullopt;
			return *iter;
		}

		auto get_node_ptr(Node& node) requires !node_is_ptr {
			return std::addressof(node);
		}
		auto get_node_ptr(Node&& node) requires !node_is_ptr = delete;

		auto get_node_ptr(Node& node) requires node_is_ptr {
			return std::addressof(*node);
		}
		auto get_node_ptr(Node&& node) requires node_is_ptr {
			return std::addressof(*node);
		}

		template<typename Iterator>
		auto& find_node(Iterator first, Iterator last) {
			auto current_node = get_node_ptr(storage_.first());
			while (first != last) {
				auto domain = *first++;
				auto next_node = get_node_ptr((*current_node)[domain]);
				current_node = next_node;
			}
			return *current_node;
		}

		template<typename Iterator>
		auto& find_node_no_add(Iterator first, Iterator last) {
			auto current_node = get_node_ptr(storage_.first());
			while (first != last) {
				auto domain = *first++;
				auto next_node = get_node_ptr(current_node->find(domain));
				if (!next_node) {
					return *current_node;
				}
				current_node = next_node;
			}
			return *current_node;
		}

		Storage storage_;
	};
}