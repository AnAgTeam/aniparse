#include "catch_amalgamated.hpp"

#include <aniparse/html/HTMLDocument.hpp>

#include <list>
#include <span>

using namespace aniparse;

bool element_contains_tags(DOMElementView element, const std::vector<std::string_view>& range) {
    size_t i = 0;
    return std::all_of(std::begin(element), std::end(element), [&range, &i](DOMElementView el) {
        return i < range.size() && el.tag_name() == range[i++];
    }) && i == range.size();
}

TEST_CASE("HTML parse") {

}

TEST_CASE("HTML document iterate") {
	constexpr std::string_view test_html = R"(<!DOCTYPE html>
    <html>
        <head></head>
        <body class="class1 class2">
            <div id="class1 class2" attr1="test">
                blabla
            </div><a id="id1" class="class3" href="#bodyContent">Jump to content</a>
            <div class="">
                <header class="">
                    <div class="">
                    <nav class="class4" aria-label="Site">
                    </nav></div>
                </header>
            </div>
        </body>
    </html>)";

    auto check_tags = [](HTMLDocument& document) -> bool {
        bool success = true;

        DOMElementView document_element = document.as_element();
        success &= document_element.tag_name() == "HTML";
        success &= std::next(std::begin(document_element), 0)->tag_name() == "HEAD";
        success &= std::next(std::begin(document_element), 1)->tag_name() == "BODY";
        success &= std::next(std::begin(document_element), 3) == std::end(document_element);

        success &= std::count_if(std::begin(document_element), std::end(document_element), [](DOMElementView el) {
            return el.tag_name() == "HEAD";
        }) == 1;
        success &= std::count_if(std::begin(document_element), std::end(document_element), [](DOMElementView el) {
            return el.tag_name() == "BODY";
        }) == 1;
        success &= std::distance(std::begin(document_element), std::end(document_element)) == 2;

        return success;
    };

    HTMLParser parser;
    HTMLDocument document = parser.parse(test_html);

    DOMElementView html_element = document.as_element();
    REQUIRE(element_contains_tags(html_element, { "HEAD", "BODY" }));

    DOMElementView body_element = *std::next(std::begin(html_element), 1);
    REQUIRE((body_element.contains_class("class1") && body_element.class_name() == "class1 class2"));
    REQUIRE(element_contains_tags(body_element, { "DIV", "A", "DIV" }));

    DOMElementView first_div_element = *std::next(std::begin(body_element), 0);
    REQUIRE(first_div_element.find_attr("attr1"));
    REQUIRE(*first_div_element.get_attr("attr1") == "test");
}