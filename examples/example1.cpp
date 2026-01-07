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
	auto parser = scanner.search_all(dom, [](auto) { return true;  });
	auto parser2 = scanner.search_all({ "com", "youtube" }, [](auto) { return true;  });
	auto parser3 = scanner.search_all({ "com", "yoube" }, [](auto) { return true;  });

	//std::string test_url = "www.youtube.com";
	//auto test_url_iter = test_url | std::views::split('.');
	//scanner.search_all(test_url_iter, [](auto) { return true;  });

	std::println("1: {}, 2: {}, 3: {}", bool(parser), bool(parser2), bool(parser3));
}

void test_domain_store() {
	struct Dummy {};
	aniparse::DomainStore<Dummy> store;

	store.insert("youtube.com", {});
	store.insert("www.youtu.be", {});
}

struct TestParser : aniparse::Parser {
	std::string identifier() const override {
		return "TestParser";
	}

	bool valid_for_url(std::string_view url) const override {
		return true;
	}

	aniparse::ParserCompatibilities compatibilities() const override {
		return {
			.flags = aniparse::compatibilities_flags::supports_anime_store
		};
	}
};

void test_parser_store() {
	//aniparse::ParserStore parser_store;

	//parser_store.add_parser("www.youtube.com", std::make_unique<TestParser>());

	//decltype(auto) parser1 = parser_store.find_for_url("https://youtube.com");
	//decltype(auto) parser2 = parser_store.find_for_url("https://www.youtube.com");

	//std::println("1: {}, 2: {}", parser1 == nullptr, parser1 == nullptr);
}

int main() {
	//test_domain_scanner();
	//test_domain_store();
	test_parser_store();
}