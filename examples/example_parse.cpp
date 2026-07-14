/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
// example_parse — the parsing half of the toolkit, with no network involved.
// It loads a saved HTML page from disk and pulls structured data out of it:
//   - CSS selectors kept in a set and cached through RequestorContext::resources(),
//     exactly the way a getter reuses them
//   - element text and attributes
//   - a JS array embedded in a <script>, extracted as JSON
// Pair it with example_net, which shows how such a page is fetched in the first
// place. Keeping the input on disk makes this example deterministic and offline.
#include <aniparse/net/Client.hpp>
#include <aniparse/ClientContext.hpp>
#include <aniparse/cache/ResourceCache.hpp>
#include <aniparse/html/HTMLParser.hpp>
#include <aniparse/html/DOMElement.hpp>
#include <aniparse/html/CompiledSelector.hpp>
#include <aniparse/html/JSParser.hpp>

#include <boost/json.hpp>

#include <fstream>
#include <iterator>
#include <print>
#include <string>

using namespace aniparse;
using namespace aniparse::html;

static std::string read_file(const std::string& path) {
	std::ifstream stream(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(stream), {});
}

// A group of selectors, compiled once and cached by type. A getter reaches its
// set through context.resources().get<T>() — the set is built on first use via
// create() and reused afterwards, so selectors are never re-parsed per call.
struct CardSelectors {
	CompiledSelector title;
	CompiledSelector card;
	CompiledSelector link;
	CompiledSelector tag;
	CompiledSelector rating;

	static CardSelectors create() {
		SelectorCompiler compiler; // transient, one per set
		return {
			compiler.compile("title"),
			compiler.compile(".card"),
			compiler.compile(".card-link"),
			compiler.compile(".tag"),
			compiler.compile(".rating"),
		};
	}
};

// Extract the card list from a parsed page. Shaped like a getter method: it takes
// a RequestorContext and pulls its selectors from the shared cache rather than
// compiling them locally — so every call reuses the same compiled set.
static void extract_cards(RequestorContext context, const HTMLDocument& document) {
	auto selectors = context.resources().get<CardSelectors>();

	if (auto title = document.query(selectors->title)) {
		std::println("Title: {}\n", title->text());
	}

	// query_all returns every match in document order; query returns the first.
	for (const DOMElementView& card : document.query_all(selectors->card)) {
		auto link = card.query(selectors->link);
		if (!link) {
			continue; // a card without a link is not one we can use
		}

		std::println("- {}", link->text());
		std::println("  url:    {}", link->get_attr("href").value_or("?"));
		std::println("  id:     {}", card.get_attr("data-id").value_or("?"));
		if (auto rating = card.query(selectors->rating)) {
			std::println("  rating: {}", rating->text());
		}

		std::print("  tags:   ");
		for (const DOMElementView& tag : card.query_all(selectors->tag)) {
			std::print("{} ", tag.text());
		}
		std::println("");
	}
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

	// A RequestorContext is what a parser's getters receive; here we build one only
	// to reach its selector cache via resources(). No request is made — the HTML
	// already came from the fixture on disk.
	RequestorContext context(std::make_shared<AsyncClient>(), nullptr, nullptr);
	extract_cards(context, document);

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
