/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
// example_parse — the parsing half of the toolkit, with no network involved.
// It loads a saved HTML page from disk and pulls structured data out of it:
//   - CSS selectors compiled once, then matched with query / query_all
//   - element text and attributes
//   - a JS array embedded in a <script>, extracted as JSON
// Pair it with example_net, which shows how such a page is fetched in the first
// place. Keeping the input on disk makes this example deterministic and offline.
#include <aniparse/html/HTMLParser.hpp>
#include <aniparse/html/DOMElement.hpp>
#include <aniparse/html/CompiledSelector.hpp>
#include <aniparse/html/JSParser.hpp>

#include <boost/json.hpp>

#include <fstream>
#include <iterator>
#include <print>
#include <string>

using namespace aniparse::html;

static std::string read_file(const std::string& path) {
	std::ifstream stream(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(stream), {});
}

int main() {
	// EXAMPLE_DATA_DIR is injected by CMake so the fixture is found regardless
	// of the working directory the example is launched from.
	std::string html = read_file(std::string(EXAMPLE_DATA_DIR) + "/manga_list.html");
	if (html.empty()) {
		std::println(stderr, "Could not read fixture from {}", EXAMPLE_DATA_DIR);
		return 1;
	}

	// Parse once. try_parse keeps a malformed document in the expected channel
	// instead of throwing.
	HTMLParser parser;
	auto document_result = parser.try_parse(html);
	if (!document_result) {
		std::println(stderr, "Parse failed: {}", document_result.error().what());
		return 1;
	}
	HTMLDocument& document = *document_result;
	std::println("Title: {}\n", document.title());

	// Compile the selectors up front; SelectorCompiler reuses one parser for the
	// whole batch. Matching afterwards (query / query_all) never fails.
	SelectorCompiler selectors;
	CompiledSelector card_sel   = selectors.compile(".card");
	CompiledSelector link_sel   = selectors.compile(".card-link");
	CompiledSelector tag_sel    = selectors.compile(".tag");
	CompiledSelector rating_sel = selectors.compile(".rating");

	// query_all returns every match in document order; query returns the first.
	for (const DOMElementView& card : document.query_all(card_sel)) {
		auto link = card.query(link_sel);
		if (!link) {
			continue; // a card without a link is not one we can use
		}

		std::println("- {}", link->text());
		std::println("  url:    {}", link->get_attr("href").value_or("?"));
		std::println("  id:     {}", card.get_attr("data-id").value_or("?"));
		if (auto rating = card.query(rating_sel)) {
			std::println("  rating: {}", rating->text());
		}

		std::print("  tags:   ");
		for (const DOMElementView& tag : card.query_all(tag_sel)) {
			std::print("{} ", tag.text());
		}
		std::println("");
	}

	// Many sites ship data as a JS literal rather than as markup. parse_json_var
	// locates `var gallery = [...]` inside the <script> and returns parsed JSON.
	std::println("\nGallery (from a <script> JS array):");
	if (auto gallery = parse_json_var("gallery", html); gallery && gallery->is_array()) {
		for (const boost::json::value& item : gallery->as_array()) {
			if (item.is_string()) {
				std::println("  {}", item.as_string().c_str());
			}
		}
	} else {
		std::println("  (not found)");
	}

	return 0;
}
