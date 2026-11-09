#pragma once
#include <aniparse/json/Conversion.hpp>
#include <boost/describe.hpp>
#include <array>
#include <string>
#include <variant>
#include <vector>

namespace json_benchmark {
struct Numbers {
    int id;
    double score;
    bool active;
    std::array<int, 8> counts;
};
BOOST_DESCRIBE_STRUCT(Numbers, (), (id, score, active, counts))
struct Author { int id; std::string name; };
BOOST_DESCRIBE_STRUCT(Author, (), (id, name))
struct Item {
    int id;
    std::string title;
    std::string description;
    std::vector<Author> authors;
    std::vector<std::string> images;
    std::optional<int> year;
    aniparse::Tolerant<int> views;
    aniparse::Tolerant<std::vector<Author>> credits;
};
BOOST_DESCRIBE_STRUCT(Item, (), (id, title, description, authors, images, year, views, credits))
struct Page { std::vector<Item> items; int total; };
BOOST_DESCRIBE_STRUCT(Page, (), (items, total))
struct Forgiving { aniparse::Tolerant<Author> author; int id; };
BOOST_DESCRIBE_STRUCT(Forgiving, (), (author, id))
struct Numeric { int id; };
BOOST_DESCRIBE_STRUCT(Numeric, (), (id))
using Choice = std::variant<Author, Numeric>;
using Tuple = std::tuple<int, std::string, std::array<int, 4>>;
struct Tree { std::vector<Tree> children; int id; };
BOOST_DESCRIBE_STRUCT(Tree, (), (children, id))
} // namespace json_benchmark
