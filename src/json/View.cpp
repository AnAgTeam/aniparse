#include "aniparse/json/View.hpp"

#include <charconv>
#include <stdexcept>

namespace aniparse::json {
namespace {
bool identifier_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool digit(char c) { return c >= '0' && c <= '9'; }
}

Selector::Selector(std::string_view path) {
    auto invalid = [] { throw std::invalid_argument("Invalid JSON selector"); };
    if (path.empty()) invalid();
    if (path == ".") return;
    std::size_t pos = 0;
    while (pos < path.size()) {
        if (path[pos] == '.') {
            const auto start = ++pos;
            if (pos == path.size() || !identifier_start(path[pos])) invalid();
            while (pos < path.size() && (identifier_start(path[pos]) || digit(path[pos]))) ++pos;
            steps_.emplace_back(std::string(path.substr(start, pos - start)));
        } else if (path[pos] == '[') {
            if (++pos == path.size()) invalid();
            if (path[pos] == '"') {
                const auto start = pos++;
                bool closed = false;
                while (pos < path.size()) {
                    const char c = path[pos++];
                    if (c == '\\') {
                        if (pos == path.size()) invalid();
                        ++pos;
                    } else if (c == '"') { closed = true; break; }
                }
                if (!closed) invalid();
                boost::system::error_code error;
                auto key = boost::json::parse(path.substr(start, pos - start), error);
                if (error || !key.is_string()) invalid();
                steps_.emplace_back(std::string(key.as_string()));
            } else if (path[pos] == '*') {
                ++pos;
                steps_.emplace_back(Wildcard{});
            } else {
                const auto start = pos;
                while (pos < path.size() && digit(path[pos])) ++pos;
                std::size_t index = 0;
                const auto parsed = std::from_chars(path.data() + start, path.data() + pos, index);
                if (start == pos || parsed.ec != std::errc{}) invalid();
                steps_.emplace_back(index);
            }
            if (pos == path.size() || path[pos++] != ']') invalid();
        } else invalid();
    }
}

View::View(boost::json::value&& document) {
    const auto& storage = document.storage();
    if (storage.is_shared() || storage == boost::json::storage_ptr{}) {
        owner_ = std::make_shared<const boost::json::value>(std::move(document));
    } else {
        owner_ = std::make_shared<const boost::json::value>(document, boost::json::storage_ptr{});
    }
    node_ = owner_.get();
}

const boost::json::object* View::object() const {
    if (const auto* value = std::get_if<const boost::json::value*>(&node_)) {
        return *value ? (*value)->if_object() : nullptr;
    }
    const auto* object = std::get_if<const boost::json::object*>(&node_);
    return object ? *object : nullptr;
}
const boost::json::array* View::array() const {
    if (const auto* value = std::get_if<const boost::json::value*>(&node_)) {
        return *value ? (*value)->if_array() : nullptr;
    }
    const auto* array = std::get_if<const boost::json::array*>(&node_);
    return array ? *array : nullptr;
}
View View::field(std::string_view key) const {
    const auto* parent = object();
    return View(owner_, parent ? parent->if_contains(key) : nullptr);
}
View View::index(std::size_t index) const {
    const auto* parent = array();
    return View(owner_, parent && index < parent->size() ? &(*parent)[index] : nullptr);
}
std::size_t View::array_size() const {
    const auto* parent = array();
    return parent ? parent->size() : 0;
}
View View::pick(const Selector& selector) const {
    View node = *this;
    for (const auto& step : selector.steps()) {
        if (const auto* key = std::get_if<std::string>(&step)) node = node.field(*key);
        else if (const auto* index = std::get_if<std::size_t>(&step)) node = node.index(*index);
        else throw std::invalid_argument("JSON pick requires a path without wildcards");
    }
    return node;
}
Selection Select::operator()(View root) const {
    std::vector<View> nodes;
    if (root.exists()) nodes.push_back(root);
    for (const auto& step : selector_.steps()) {
        std::vector<View> next;
        for (const auto& node : nodes) {
            if (const auto* key = std::get_if<std::string>(&step)) {
                auto child = node.field(*key);
                if (child.exists()) next.push_back(std::move(child));
            } else if (const auto* index = std::get_if<std::size_t>(&step)) {
                auto child = node.index(*index);
                if (child.exists()) next.push_back(std::move(child));
            } else {
                for (std::size_t i = 0; i < node.array_size(); ++i) next.push_back(node.index(i));
            }
        }
        nodes = std::move(next);
    }
    return Selection(std::move(nodes));
}
} // namespace aniparse::json
