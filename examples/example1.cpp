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
#include <fstream>

#include <aniparse/html/HTMLParser.hpp>
#include <aniparse/html/DOMAttributes.hpp>

using namespace aniparse;
using namespace aniparse::html;

constexpr const char html_test_file_path[] = R"(D:\example_html.html)";

void test_aniparse_html() {
	std::string html_content;
	{
		std::ifstream stream(html_test_file_path);
		stream.seekg(0, std::ios::end);
		html_content.resize(stream.tellg());
		stream.seekg(0);
		stream.read(html_content.data(), html_content.size());
	}

	HTMLParser parser;
	HTMLDocument document = parser.parse(html_content);

	//std::list<int> ltest = { 1, 2, 3 };
	//auto liter = std::begin(ltest);
	//auto last_liter = std::next(liter, 3);
	//auto prev_llast = std::prev(liter);

	//std::optional<DOMElementView> element = document.find_first_by_class("vector-header");
	//if (!element) {
	//	return;
	//}

	auto iter = DOMElementWalkIterator(document.as_element());
	iter = std::find_if(iter, DOMElementWalkIterator{}, [](const DOMElementView& element) {
		//std::println("{}", element.class_name());
		return element.contains_class("vector-dropdown-label");
	});
	if (iter == DOMElementWalkIterator{}) {
		return;
	}

	auto filtered = document.as_element() | std::views::filter([](const DOMElementView& el) {
		return el.contains_class("vector-dropdown-content");
	});
	for (auto& element : filtered) {
		std::println("Found: {}", element.tag_name());
	}

	std::println("body child: {}", iter->tag_name());

	for (auto& attr : iter->attributes()) {
		std::println("attr {}={}", attr.name(), attr.value());
	}

	auto attr = iter->find_attr("id");
	std::println("Find id: {}", attr ? attr->value() : "nullopt");
	attr = iter->find_attr("bbbb");
	std::println("Find bbbb: {}", attr ? attr->value() : "nullopt");
}

int main() {
	test_aniparse_html();
}