/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/html/HTMLParser.hpp>
#include <aniparse/html/HTMLDocument.hpp>
#include <aniparse/html/DOMElement.hpp>
#include <aniparse/html/DOMAttributes.hpp>
#include <aniparse/html/CompiledSelector.hpp>

#include <string>

using namespace aniparse::html;

namespace {

constexpr std::string_view selector_test_html = R"(<!DOCTYPE html>
<html>
    <head></head>
    <body>
        <div id="content">
            <a class="related_info" href="/x">Hello</a>
            <a href="/y">Bye</a>
            <span class="tag">t1</span>
            <ul>
                <li class="item">one</li>
                <li class="item">two</li>
            </ul>
        </div>
        <div id="side">
            <a class="related_info" href="/z">Side</a>
        </div>
    </body>
</html>)";

} // namespace

TEST_CASE("CompiledSelector::compile reports invalid selectors") {
    // Well formed selectors compile.
    CHECK(CompiledSelector::compile("#content a.related_info").has_value());
    CHECK(CompiledSelector::compile(".item").has_value());
    CHECK(CompiledSelector::compile("a[href]").has_value());

    // An unclosed attribute selector is malformed and must fail rather than
    // silently behaving like "matches nothing".
    auto bad = CompiledSelector::compile("a[");
    REQUIRE_FALSE(bad.has_value());
    CHECK_FALSE(std::string(bad.error().what()).empty());
}

TEST_CASE("SelectorCompiler batch compiles reusing one parser") {
    SelectorCompiler compiler;

    CompiledSelector a = compiler.compile("#content a");
    CompiledSelector b = compiler.compile(".item");
    CompiledSelector c = compiler.compile("div#side");

    CHECK(a);
    CHECK(b);
    CHECK(c);

    // Throwing overload surfaces a bad selector loudly (used by selector tables).
    CHECK_THROWS_AS(compiler.compile("a["), SelectorParseError);
    // The compiler stays usable after a failed parse.
    CHECK(compiler.compile("span.tag"));
}

TEST_CASE("HTMLDocument::query finds the first matching element") {
    HTMLDocument document = parse_html(selector_test_html);

    auto sel = CompiledSelector::compile("#content a.related_info");
    REQUIRE(sel.has_value());

    std::optional<DOMElementView> found = document.query(*sel);
    REQUIRE(found.has_value());
    CHECK(found->tag_name() == "A");
    CHECK(found->get_attr("href").value_or("") == "/x");
    CHECK(found->text() == "Hello");
}

TEST_CASE("HTMLDocument::query returns nullopt when nothing matches") {
    HTMLDocument document = parse_html(selector_test_html);

    auto sel = CompiledSelector::compile(".does-not-exist");
    REQUIRE(sel.has_value());

    CHECK_FALSE(document.query(*sel).has_value());
}

TEST_CASE("HTMLDocument::query_all returns every match in document order") {
    HTMLDocument document = parse_html(selector_test_html);

    auto anchors = CompiledSelector::compile("a");
    auto items   = CompiledSelector::compile("li.item");
    REQUIRE(anchors.has_value());
    REQUIRE(items.has_value());

    std::vector<DOMElementView> all_anchors = document.query_all(*anchors);
    REQUIRE(all_anchors.size() == 3);
    CHECK(all_anchors[0].get_attr("href").value_or("") == "/x");
    CHECK(all_anchors[1].get_attr("href").value_or("") == "/y");
    CHECK(all_anchors[2].get_attr("href").value_or("") == "/z");

    CHECK(document.query_all(*items).size() == 2);
}

TEST_CASE("HTMLDocument::query_all deduplicates across a selector list") {
    HTMLDocument document = parse_html(selector_test_html);

    // The two .related_info anchors match both "a" and ".related_info".
    // With MATCH_FIRST each node is reported once: 3 anchors, not 5.
    auto sel = CompiledSelector::compile("a, .related_info");
    REQUIRE(sel.has_value());

    CHECK(document.query_all(*sel).size() == 3);
}

TEST_CASE("DOMElementView::query searches only descendants of the element") {
    HTMLDocument document = parse_html(selector_test_html);

    auto content_sel = CompiledSelector::compile("#content");
    auto anchor_sel  = CompiledSelector::compile("a");
    auto self_sel    = CompiledSelector::compile("#content");
    REQUIRE(content_sel.has_value());
    REQUIRE(anchor_sel.has_value());
    REQUIRE(self_sel.has_value());

    std::optional<DOMElementView> content = document.query(*content_sel);
    REQUIRE(content.has_value());

    // Only the two anchors inside #content, not the one in #side.
    CHECK(content->query_all(*anchor_sel).size() == 2);

    // The root element itself is not matched, only its descendants.
    CHECK_FALSE(content->query(*self_sel).has_value());
}
