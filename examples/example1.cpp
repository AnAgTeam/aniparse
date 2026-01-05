#include <aniparse/AniParse.hpp>

#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <span>
#include <optional>

#include <print>

template<
	typename DomainParser,
	typename Char = char
>
class DomainNode {
public:
	using DomainStringView = std::basic_string_view<Char>;
	using DomainString = std::basic_string<Char>;

	using Node = std::unique_ptr<DomainNode>;
	using NodesContainer = std::map<DomainString, Node, std::less<>>;
	using ParserContainer = std::vector<DomainParser>;

	static constexpr Node null_node;

	Node& find(DomainStringView name) {
		auto iter = nexts_.find(name);
		return iter != nexts_.end() ? iter->second : null_node;
	}

	//Node& operator[](const DomainStringView& name) {
	//	auto iter = nexts_.find(name);
	//	return iter != nexts_.end() ? iter->second : insert(DomainString(name));
	//}

	template<typename T>
	Node& operator[](T name) {
		auto iter = nexts_.find(name);
		return iter != nexts_.end() ? iter->second : insert(std::forward<T>(name));
	}

	Node& insert(DomainString name) {
		Node& new_node = nexts_[std::move(name)] = std::make_unique<DomainNode>();
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

private:

	ParserContainer parsers_;
	NodesContainer nexts_;
};

template<
	typename DomainParser,
	typename Char = char
>
class DomainStorage {
public:

	using Node = std::unique_ptr<DomainNode<DomainParser, Char>>;

	Node& first() {
		return domains_start_;
	}

	Node& last() {
		return Node::null_node;
	}

private:

	Node domains_start_ = std::make_unique<DomainNode<DomainParser, Char>>();
};

template<typename DomainParser, typename Char = char, typename Storage = DomainStorage<DomainParser, Char>>
	//requires requires (std::remove_pointer_t<DomainParser> parser, std::string_view url) { parser.is_url_supported(url); }
class DomainScanner {
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
		auto& parsers = find_node(first, last).parsers();
		auto iter = std::find_if(std::begin(parsers), std::end(parsers), pred);
		if (iter == parsers.end()) return std::nullopt;
		return *iter;
	}

	template<typename T, typename Predicate>
	std::optional<DomainParser> search_all(std::initializer_list<T> range, Predicate pred) {
		return search_all(std::begin(range), std::end(range), pred);
	}

	template<std::ranges::range Range, typename Predicate>
	std::optional<DomainParser> search_all(Range range, Predicate pred) {
		return search_all(std::begin(range), std::end(range), pred);
	}

	template<typename Iterator, typename Predicate>
	std::optional<DomainParser> search_all(Iterator first, Iterator last, Predicate pred) {
		do {
			if (auto result = search(first, last, pred))
				return result;
		} while (last-- != first);
		return std::nullopt;
	}

private:

	static constexpr bool node_is_ptr = requires (typename Storage::Node node) { *node; };

	auto get_node_ptr(typename Storage::Node& node) requires !node_is_ptr {
		return std::addressof(node);
	}

	auto get_node_ptr(typename Storage::Node& node) requires node_is_ptr {
		return std::addressof(*node);
	}

	template<typename Iterator>
	auto& find_node(Iterator first, Iterator last) {
		auto current_node = get_node_ptr(storage_.first());
		while (first != last) {
			auto domain = *first++;
			current_node = get_node_ptr((*current_node)[domain]);
		}
		return *current_node;
	}

	Storage storage_;
};

int main() {
	struct Dummy {};
	DomainScanner<std::shared_ptr<Dummy>, char> scanner;

	scanner.add_domain_parser({ "com", "youtube" }, std::make_shared<Dummy>());
	scanner.add_domain_parser({ "com", "yoube", "www" }, std::make_shared<Dummy>());

	std::vector dom = { "com", "youtube", "www" };
	auto parser = scanner.search_all(dom, [](auto) { return true;  });
	auto parser2 = scanner.search_all({ "com", "youtube" }, [](auto) { return true;  });
	auto parser3 = scanner.search_all({ "com", "yoube" }, [](auto) { return true;  });
	
	std::println("1: {}, 2: {}, 3: {}", bool(parser), bool(parser2), bool(parser3));
}