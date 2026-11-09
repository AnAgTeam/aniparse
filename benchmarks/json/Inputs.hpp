#pragma once

#include <boost/json.hpp>
#include <string>

namespace json_benchmark {
inline boost::json::value page(std::size_t count) {
    boost::json::array items;
    for (std::size_t index = 0; index < count; ++index) {
        items.push_back(boost::json::object{
            {"id", index}, {"title", "A title long enough to require owned string storage"},
            {"description", std::string(160, 'x')},
            {"authors", boost::json::array{
                {{"id", 1}, {"name", "An author with a sufficiently long name"}},
                {{"id", 2}, {"name", "Another author with a sufficiently long name"}}}},
            {"images", boost::json::array{"https://example.test/images/cover.jpg", "https://example.test/images/back.jpg"}},
            {"year", 2026}, {"views", 1000}, {"credits", nullptr}});
    }
    return boost::json::object{{"items", std::move(items)}, {"total", count}};
}

} // namespace json_benchmark
