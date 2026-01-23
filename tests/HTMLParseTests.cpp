/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/html/HTMLParser.hpp>
#include <aniparse/html/DOMNode.hpp>
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

constexpr std::string_view inner_test_html = R"raw(<!DOCTYPE html>
    <html>
        <head></head>
        <body class="class1 class2 class3-1">
            <div id="id1" class="class1 class2" attr1="test">
                blabla <b>bold</b>
                <img><div><a></a></div></img>
            </div><a id="id1" class="class3" href="#bodyContent">Jump to content</a>
            <div class="">
                <header class="">
                    <div class="class2">
                    <nav class="class4" aria-label="Site">
                    </nav></div>
                </header>
                <footer></footer>
            </div>
            <img></img>
        </body>
    </html>)raw";

TEST_CASE("HTML walk with inner") {
    HTMLParser parser;
    HTMLDocument document = parser.parse(inner_test_html);

    DOMElementView body_element = document.body();
    auto div_element = *std::next(std::begin(body_element), 0);
    CHECK(div_element);

    DOMElementWalkIterator walk_iterator(div_element);
    REQUIRE(std::distance(walk_iterator, DOMElementWalkIterator{}) == 4);

    // the last element
    auto div_a_element = div_element.find("A");
    CHECK(div_a_element);
    REQUIRE(std::distance(DOMElementWalkIterator(*div_a_element), DOMElementWalkIterator{}) == 0);
}

TEST_CASE("HTML find with inner") {
    HTMLParser parser;
    HTMLDocument document = parser.parse(inner_test_html);

    DOMElementView html_element = document.as_element();
    auto found_div_element = html_element.find("DIV");
    CHECK(found_div_element);
    REQUIRE(found_div_element->find_all("IMG").size() == 1);

    // the last element
    auto div_a_element = found_div_element->find("A");
    CHECK(div_a_element);
    REQUIRE(!div_a_element->find("A"));
    REQUIRE(!div_a_element->find("class", "class1"));
    REQUIRE(div_a_element->find_all("DIV").size() == 0);
    REQUIRE(div_a_element->find_all("id", "id1").size() == 0);
}

TEST_CASE("HTML element text") {
    HTMLParser parser;
    HTMLDocument document = parser.parse(inner_test_html);

    DOMElementView html_element = document.as_element();
    auto found_div_element = html_element.find("attr1", "test");
    CHECK(found_div_element);
    REQUIRE(found_div_element->content_text() == R"(
                blabla bold
                
            )");
    auto div_img_element = found_div_element->find("IMG");
    CHECK(div_img_element);
    REQUIRE(div_img_element->content_text().empty());
}

TEST_CASE("HTML empty element iteration") {
    DOMElementView element;
    REQUIRE(std::distance(std::begin(element), std::end(element)) == 0);
    REQUIRE(std::distance(DOMElementWalkIterator(element), DOMElementWalkIterator{}) == 0);
}


// Nodes

template<typename Iter>
bool all_nodes_is(Iter first, Iter last, const std::vector<std::string_view>& range) {
    size_t i = 0;
    return std::all_of(first, last, [&range, &i](DOMNodeView node) {
        if (i >= range.size()) return false;
        auto& range_value = range[i++];
        return node.is_element() && range_value == "ELEMENT"
            || node.is_text() && range_value == "TEXT";
    });
}

template<std::ranges::range Range>
bool all_nodes_is(Range nodes_range, const std::vector<std::string_view>& range) {
    return all_nodes_is(begin(nodes_range), end(nodes_range), range);
}

TEST_CASE("HTML node constructon") {
    constexpr std::string_view test_html = R"(<!DOCTYPE html>
    <html>
        <head></head>
        <body class="class1 class2 class3-1">
            <div id="id1" class="class1 class2" attr1="test">
                blabla
            </div><a></a>
            <img></img></body>
    </html>)";

    HTMLParser parser;
    HTMLDocument document = parser.parse(test_html);

    DOMNodeView body_element = document.body();
    REQUIRE(all_nodes_is(DOMNodeWalkIterator(body_element), {
        "TEXT", "ELEMENT", "TEXT", "ELEMENT",
        "TEXT", "ELEMENT", "TEXT", "ELEMENT"
    }));
}

TEST_CASE("HTML text") {
    constexpr std::string_view test_html = R"(<!DOCTYPE html>
    <html>
        <head></head>
        <body class="class1 class2 class3-1">
            <div id="id1" class="class1 class2" attr1="test">
                blabla
            </div><a>. 
hello</a><br>new line
            <img></img></body>
    </div>
    </html>)";

    constexpr std::string_view test_html2 = "<!DOCTYPE html>"
        "<html>"
        "<body><div class=\"tags\">"
        "    <b>Desc</b>: <span>   </span><div id=\"news-id-53768\">Simple    man, will it help      him...<br>Link    \v  -\n\t <a href=\"#\">#</a>"
        "    </div>"
        "</div>)";

    HTMLParser parser;
    HTMLDocument document = parser.parse(test_html);

    REQUIRE(document.body().text() == "blabla . hello\nnew line");
    auto found_img_element = document.body().find("IMG");
    REQUIRE((found_img_element && found_img_element->text().empty()));

    HTMLDocument document2 = parser.parse(test_html2);
    auto found_div_element = document2.body().find("class", "tags");
    CHECK(found_div_element);
    REQUIRE(found_div_element->text() == "Desc: Simple man, will it help him...\nLink - #");
}