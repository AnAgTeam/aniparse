#include <aniparse/AniParse.hpp>
#include <aniparse/ParserStore.hpp>
#include <aniparse/DomainScanner.hpp>

#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <span>
#include <optional>

#include <print>

using aniparse::DomainScanner;

void test_domain_scanner() {
	struct Dummy {};
	DomainScanner<std::shared_ptr<Dummy>, char> scanner;

	scanner.add_domain_parser({ "com", "youtube" }, std::make_shared<Dummy>());
	scanner.add_domain_parser({ "com", "yoube", "www" }, std::make_shared<Dummy>());

	std::vector dom = { "com", "youtube", "www" };
	std::vector dom2 = { "com", "youtube", "www", "unk" };
	auto parser = scanner.search_all(std::begin(dom), std::end(dom), [](auto) { return true;  });
	auto parser2 = scanner.search_all({ "com", "youtube" }, [](auto) { return true;  });
	auto parser3 = scanner.search_all({ "com", "yoube" }, [](auto) { return true;  });
	auto parser4 = scanner.search_all(dom2, [](auto) { return true;  });

	std::string test_url = "www.youtube.com";
	auto test_url_iter = test_url
		| std::views::reverse
		| std::views::split('.')
		| std::views::transform(std::views::reverse);
	scanner.search_all(test_url_iter, [](auto) { return true;  });

	std::println("1: {}, 2: {}, 3: {}", bool(parser), bool(parser2), bool(parser3));
}

struct TestParser : aniparse::Parser {
	std::string identifier() const override {
		return "TestParser";
	}

	bool valid_for_url(std::string_view url) const override {
		return true;
	}

	aniparse::ParserCompatibilities compatibilities() const override {
		using namespace aniparse::compatibilities_flags;
		return {
			.flags = supports_anime_store | supports_manga_store
		};
	}

	void emplace_domains(aniparse::EmplaceDomainsContext& context) const override {
		context.add_domain("www.youtube.com");
		context.add_domain("youtu.be");
	}
};

void test_parser_store() {
	aniparse::ParserStore parser_store;
	
	parser_store.add_parser(std::make_unique<TestParser>());
	parser_store.add_parser(std::make_unique<TestParser>());
	
	decltype(auto) parser1 = parser_store.find_for_url("https://youtube.com");
	decltype(auto) parser2 = parser_store.find_for_url("https://www.youtube.com");
	decltype(auto) parser3 = parser_store.find_for_url("https://youtu.be");
	decltype(auto) parser4 = parser_store.find_for_url("http://youtu.be");

	decltype(auto) key_parser = parser_store.find_by_key("TestParser");
	
	std::println("1: {}, 2: {}", parser1 != nullptr, parser2 != nullptr);
}

int main() {
	//test_domain_scanner();
	test_parser_store();
}