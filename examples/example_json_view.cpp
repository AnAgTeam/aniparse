#include <aniparse/json/View.hpp>
#include <iostream>
#include <aniparse/utility/Number.hpp>

int main() {
    namespace json = aniparse::json;
    // Temporary JSON is moved into shared ownership; lvalue JSON would be borrowed.
    json::View root(boost::json::parse(R"({
        "authors": [
            {"name": "Rio", "active": true},
            {"name": "Riona", "active": true},
            {"name": "Other", "active": false}
        ]
    })"));
    std::cout << root.pick(".authors[0].name").as<std::string>().value_or("Unknown") << '\n';

    // Selection retains the document; scalar extraction returns owned strings.
    // Conditions remain ordinary C++, and selectors can be compiled once and reused.
    const json::Selector name(".name");
    auto names = root
        | json::views::select(".authors[*]")
        | std::views::filter([](json::View author) {
              return author.pick(".active").as<bool>().value_or(false);
          })
        | json::views::select(name)
        | json::views::values<std::string>()
        | std::views::filter([](const std::string& value) { return value.starts_with("Rio"); });
    root = {};
    for (const auto& value : names) std::cout << value << '\n';

    // C++20: collect the selected characters explicitly, then parse the string.
    const auto text = json::View(boost::json::value("42votes")).as<std::string>().value_or("");
    const auto prefix = text | std::views::take_while([](char c) { return c >= '0' && c <= '9'; });
    std::string digits;
    for (char c : prefix) digits.push_back(c);
    std::cout << aniparse::number::parse_or<int>(digits, 0) << '\n';
}
