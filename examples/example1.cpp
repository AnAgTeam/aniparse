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

#include <tinyxml2.h>

#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>

#include <aniparse/html/HTMLDocument.hpp>

using namespace aniparse;

constexpr const char html_test_file_path[] = R"(D:\example_html.html)";

void test_html_parse() {
	//tinyxml2::XMLDocument document;

	////std::string test_document(test_html_text);
	////document.Parse(test_document.c_str(), test_document.size());

	//document.LoadFile(html_test_file_path);

	//document.FirstChildElement("body");

	std::string html_content;
	{
		std::ifstream stream(html_test_file_path);
		stream.seekg(0, std::ios::end);
		html_content.resize(stream.tellg());
		stream.seekg(0);
		stream.read(html_content.data(), html_content.size());
	}
	lxb_html_parser_t* parser = lxb_html_parser_create();
	lxb_html_parser_init(parser);
	lxb_html_document* document = lxb_html_parse(parser, reinterpret_cast<const lxb_char_t*>(html_content.c_str()), html_content.size());

	lxb_dom_element_t* body_element = lxb_dom_interface_element(document->body);
	size_t body_class_size;
	const lxb_char_t* body_class_name = lxb_dom_element_class(body_element, &body_class_size);

	lxb_dom_node_t* body_node = lxb_dom_interface_node(body_element);
	//for (auto node = body_node->first_child; node != body_node->last_child; node = node->next) {
	//	if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
	//		auto node_element = lxb_dom_interface_element(node);
	//
	//		size_t node_tag_size;
	//		const lxb_char_t* node_tag_name = lxb_dom_element_tag_name(node_element, &node_tag_size);
	//		std::println("body child: {}", reinterpret_cast<const char*>(node_tag_name));
	//	}
	//	else if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
	//		//auto node_text = lxb_dom_interface_text(node);
	//
	//		size_t node_text_size;
	//		const lxb_char_t* node_text = lxb_dom_node_text_content(node, &node_text_size);
	//		std::println("body child (text): {}", reinterpret_cast<const char*>(node_text));
	//	}
	//}

	lxb_dom_collection_t* dom_collection = lxb_dom_collection_create(&document->dom_document);
	lxb_dom_collection_init(dom_collection, 100);

	lxb_dom_node_by_class_name(body_node, dom_collection, reinterpret_cast<const lxb_char_t*>("vector-header"), 13);

	auto node_element = lxb_dom_interface_element(dom_collection->array.list[0]);

	size_t node_tag_size;
	const lxb_char_t* node_tag_name = lxb_dom_element_tag_name(node_element, &node_tag_size);
	std::println("body child: {}", reinterpret_cast<const char*>(node_tag_name));
}

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
	//test_html_parse();
	test_aniparse_html();
}