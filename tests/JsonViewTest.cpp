#include "catch_amalgamated.hpp"
#include <aniparse/json/View.hpp>
#include <array>
#include <stdexcept>

namespace json = aniparse::json;

TEST_CASE("JSON views borrow lvalues and retain moved rvalues", "[json-view]") {
    auto source = boost::json::parse(R"({"name":"Rio"})");
    {
        json::View borrowed(source);
        CHECK(borrowed.pick(".name").as<std::string>() == "Rio");
        const auto& constant = source;
        CHECK(json::View(constant).pick(".name").as<std::string>() == "Rio");
        json::View object(source.as_object());
        CHECK(object.pick(".").field("name").as<std::string>() == "Rio");
        CHECK(object.exists());
        CHECK_FALSE(object.as<std::string>());
    }
    json::View root(std::move(source));
    CHECK(source.is_null());
    auto child = root.pick(".name");
    root = {};
    CHECK(child.as<std::string>() == "Rio");
    auto moved = std::move(child);
    CHECK(child.as<std::string>() == "Rio");
    CHECK(moved.as<std::string>() == "Rio");

    // A raw resource pointer is not ownership: rvalues must detach from it.
    auto retained = [] {
        boost::json::monotonic_resource resource;
        auto document = boost::json::parse(R"({"name":"resource-owned"})", &resource);
        return json::View(std::move(document)).pick(".name");
    }();
    CHECK(retained.as<std::string>() == "resource-owned");
    auto shared = [] {
        auto storage = boost::json::make_shared_resource<boost::json::monotonic_resource>();
        auto document = boost::json::parse(R"({"name":"shared-resource"})", storage);
        auto view = json::View(std::move(document));
        CHECK(document.is_null());
        return view.pick(".name");
    }();
    CHECK(shared.as<std::string>() == "shared-resource");
}

TEST_CASE("JSON object and array inputs preserve ownership category", "[json-view]") {
    static_assert(!std::is_constructible_v<json::View, const boost::json::value&&>);
    static_assert(!std::is_constructible_v<json::View, const boost::json::object&&>);
    static_assert(!std::is_constructible_v<json::View, const boost::json::array&&>);
    boost::json::array array{1, 2};
    const json::View borrowed(array);
    CHECK(borrowed.array_size() == 2);
    CHECK(borrowed.pick("[1]").as<int>() == 2);
    CHECK_FALSE(borrowed.field("name").exists());
    auto range = borrowed | json::views::select("[*]") | json::views::values<int>();
    CHECK(*range.begin() == 1);
    auto owned_array = json::View(boost::json::array{3, 4});
    CHECK(owned_array.pick("[1]").as<int>() == 4);
    auto child = json::View(boost::json::object{{"name", "temporary object"}}).pick(".name");
    CHECK(child.as<std::string>() == "temporary object");
    auto owned_resource_array = [] {
        boost::json::monotonic_resource storage;
        boost::json::array input({"long enough string to require allocated JSON storage"}, &storage);
        return json::View(std::move(input)) | json::views::select("[*]");
    }();
    CHECK(owned_resource_array.begin()->as<std::string>() == "long enough string to require allocated JSON storage");
}

TEST_CASE("JSON selectors distinguish missing data from invalid paths", "[json-view]") {
    json::View root(boost::json::parse(R"({"a.b":[{"name":"Rio"}],"null":null,"":3,"quote\"":"ok"})"));
    CHECK(root.pick(R"(["a.b"][0].name)").as<std::string>() == "Rio");
    CHECK(root.pick(R"([""])").as<int>() == 3);
    CHECK(root.pick(R"(["quote\""])").as<std::string>() == "ok");
    CHECK(root.pick(".null").exists());
    CHECK_FALSE(root.pick(".null").as<std::string>());
    CHECK_FALSE(root.pick(".missing.child[0]").exists());
    CHECK_FALSE(root.pick(R"(["a.b"][99])").exists());
    CHECK_FALSE(root.pick(".null[0]").exists());
    CHECK(root.pick(".").exists());
    CHECK_THROWS_AS(root.pick(".missing[*]"), std::invalid_argument);
    for (auto path : {"", "authors", ".", "..a", ".a.", "[-1]", "[", "[]", "[1", "[1]x", "[999999999999999999999999999]", "[\"bad\\q\"]"}) {
        if (std::string_view(path) != ".") CHECK_THROWS_AS(json::Selector(path), std::invalid_argument);
    }
}

TEST_CASE("JSON scalar conversion is explicit and range checked", "[json-view]") {
    json::View root(boost::json::parse(R"({"n":-1,"u":18446744073709551615,"fraction":1.5,"text":"12","false":false,"zero":0,"empty":"","nul":"a\u0000b"})"));
    CHECK_FALSE(root.pick(".n").as<unsigned>());
    CHECK_FALSE(root.pick(".u").as<std::int64_t>());
    CHECK(root.pick(".u").as<std::uint64_t>() == UINT64_MAX);
    CHECK_FALSE(root.pick(".fraction").as<int>());
    CHECK_FALSE(root.pick(".text").as<int>());
    CHECK(root.pick(".n").as<double>() == -1.0);
    CHECK(root.pick(".false").as<bool>().value_or(true) == false);
    CHECK(root.pick(".zero").as<int>().value_or(42) == 0);
    CHECK(root.pick(".empty").as<std::string>().value_or("fallback").empty());
    CHECK(root.pick(".nul").as<std::string>()->size() == 3);
    CHECK_FALSE(json::View(boost::json::value(256)).as<std::uint8_t>());
    CHECK_FALSE(json::View(boost::json::value(std::numeric_limits<double>::infinity())).as<double>());
}

TEST_CASE("JSON selections compose with standard ranges and survive temporaries", "[json-view]") {
    auto names = json::View(boost::json::parse(R"({"groups":[{"authors":[{"name":"Rio","active":true},{"name":"Other","active":false}]},{"authors":[{"name":"Riona","active":true},{"name":42,"active":true},null]}]})"))
        | json::views::select(".groups[*].authors[*]")
        | std::views::filter([](json::View node) { return node.pick(".active").as<bool>().value_or(false); })
        | json::views::select(".name")
        | json::views::values<std::string>()
        | std::views::filter([](const std::string& name) { return name.starts_with("Rio"); });
    std::vector<std::string> actual;
    for (auto name : names) actual.push_back(std::move(name));
    CHECK(actual == std::vector<std::string>{"Rio", "Riona"});
    auto copied = names;
    CHECK(*copied.begin() == "Rio");
    auto node = [] { auto nodes = json::View(boost::json::parse("[1,2]")) | json::views::select("[*]"); return *nodes.begin(); }();
    CHECK(node.as<int>() == 1);
    CHECK((json::View{} | json::views::select(".")).empty());
    CHECK((json::View(boost::json::parse("null")) | json::views::select(".")).size() == 1);
    std::array roots{json::View(boost::json::parse("[1,2]")), json::View(boost::json::parse("[3]"))};
    auto selected = roots | json::views::select("[*]");
    CHECK(selected.size() == 3);
    CHECK((selected.begin() + 2)->as<int>() == 3);
}



TEST_CASE("JSON container predicates distinguish empty containers from missing and wrong types", "[json-view]") {
    const auto root = json::View(boost::json::parse(R"({"array":[],"object":{},"null":null,"scalar":0})"));
    CHECK(root.is_object());
    CHECK_FALSE(root.is_array());
    CHECK(root.field("array").is_array());
    CHECK_FALSE(root.field("array").is_object());
    CHECK(root.field("object").is_object());
    CHECK_FALSE(root.field("object").is_array());
    for (const auto key : {"missing", "null", "scalar"}) {
        CHECK_FALSE(root.field(key).is_array());
        CHECK_FALSE(root.field(key).is_object());
    }
    CHECK_FALSE(json::View{}.is_array());
    CHECK_FALSE(json::View{}.is_object());
    boost::json::object object;
    boost::json::array array;
    CHECK(json::View(object).is_object());
    CHECK(json::View(array).is_array());
    CHECK(json::View(boost::json::object{}).is_object());
    CHECK(json::View(boost::json::array{}).is_array());
}
