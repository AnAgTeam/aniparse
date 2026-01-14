#include "catch_amalgamated.hpp"

#include <aniparse/html/HTMLParser.hpp>
#include <aniparse/html/DOMAttributes.hpp>

#include <list>
#include <span>
#include <print>

using namespace aniparse::html;

constexpr std::string_view iterator_test_html = R"(<!DOCTYPE html>
    <html>
        <head></head>
        <body class="class1 class2 class3-1">
            <div id="id1" class="class1 class2" attr1="test">
                blabla
            </div><a id="id1" class="class3" href="#bodyContent">Jump to content</a>
            <div class="">
                <header class="">
                    <div class="class2">
                    <nav class="class4" aria-label="Site">
                    </nav></div>
                </header>
                <footer></footer>
            </div>
        </body>
    </html>)";

template<std::ranges::range Range>
bool element_contains_tags(Range elements_range, const std::vector<std::string_view>& range) {
    size_t i = 0;
    return std::all_of(std::begin(elements_range), std::end(elements_range), [&range, &i](DOMElementView el) {
        return i < range.size() && el.tag_name() == range[i++];
    }) && i == range.size();
}

template<typename Iter>
bool element_contains_tags(Iter first, Iter last, const std::vector<std::string_view>& range) {
    size_t i = 0;
    return std::all_of(first, last, [&range, &i](DOMElementView el) {
        return i < range.size() && el.tag_name() == range[i++];
    }) && i == range.size();
}

TEST_CASE("HTML parse") {
    constexpr std::string_view invalid_doctype_html = R"(
    <html><!DOCTYPE html>
        <head></head>
        <body class="class1 class2"></body>
    </html>)";

    constexpr std::string_view invalid_unclosed_html = R"(<!DOCTYPE html>
    <html>
        <
        <body class="class1 class2"></body>
    <html>)";

    constexpr std::string_view invalid_no_html = R"(<!DOCTYPE html>
        <>
        <body class="class1 class2"></body>)";
    
    HTMLParser parser;
    REQUIRE_THROWS_AS(parser.parse(invalid_doctype_html), HTMLParseError);
}

TEST_CASE("HTML document iterate") {
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
    HTMLDocument document = parser.parse(iterator_test_html);

    DOMElementView html_element = document.as_element();
    REQUIRE(element_contains_tags(html_element, { "HEAD", "BODY" }));

    DOMElementView body_element = *std::next(std::begin(html_element), 1);
    REQUIRE((body_element.contains_class("class1") && body_element.class_name() == "class1 class2 class3-1"));
    REQUIRE(element_contains_tags(body_element, { "DIV", "A", "DIV" }));

    DOMElementView first_div_element = *std::next(std::begin(body_element), 0);
    REQUIRE(first_div_element.find_attr("attr1"));
    REQUIRE(*first_div_element.get_attr("attr1") == "test");
}

TEST_CASE("DOMDocument walk") {
    HTMLParser parser;
    HTMLDocument document = parser.parse(iterator_test_html);

    DOMElementView html_element = document.as_element();
    struct DOMElementWalker {
        using iterator_type = DOMElementWalkIterator;

        auto begin() const {
            return DOMElementWalkIterator(element_);
        }

        auto end() const {
            return DOMElementWalkIterator{};
        }

        DOMElementView element_;
    };

    DOMElementWalker walker(html_element);
    REQUIRE(element_contains_tags(walker, {
        "HEAD", "BODY", "DIV", "A", "DIV", "HEADER", "DIV", "NAV", "FOOTER"
    }));
}

TEST_CASE("HTML find, find_all") {
    HTMLParser parser;
    HTMLDocument document = parser.parse(iterator_test_html);

    DOMElementView html_element = document.as_element();
    auto found_class_element = html_element.find("class", "class2");
    REQUIRE((found_class_element && found_class_element->tag_name() == "BODY"));
    REQUIRE(!html_element.find("class", "not-a-class"));
    REQUIRE(html_element.find_all("class", "class2").size() == 3);
    REQUIRE(html_element.find_all("class", "not-a-class").size() == 0);

    auto found_id_element = html_element.find("id", "id1");
    REQUIRE((found_id_element && found_id_element->tag_name() == "DIV"));
    REQUIRE(!html_element.find("id", "not-an-id"));
    REQUIRE(html_element.find_all("id", "id1").size() == 2);
    REQUIRE(html_element.find_all("id", "not-an-id").size() == 0);

    auto found_custom_element = html_element.find("aria-label", "Site");
    REQUIRE((found_custom_element && found_custom_element->tag_name() == "NAV"));
    REQUIRE(!html_element.find("not-an-attr", ""));
    REQUIRE(html_element.find_all("aria-label", "Site").size() == 1);
    REQUIRE(html_element.find_all("not-an-attr", "id1").size() == 0);
}

TEST_CASE("HTML find, find_all (tags)") {
    HTMLParser parser;
    HTMLDocument document = parser.parse(iterator_test_html);

    DOMElementView html_element = document.as_element();
    auto found_div_element = html_element.find("DIV");
    REQUIRE((found_div_element && found_div_element->tag_name() == "DIV"));
    REQUIRE(!html_element.find("ELE"));
    REQUIRE(html_element.find_all("DIV").size() == 3);
    REQUIRE(html_element.find_all("ELEMENT").size() == 0);
}