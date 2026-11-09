#include "aniparse/json/detail/DecodeContext.hpp"
#include <boost/json.hpp>
#include <vector>

namespace aniparse::json {
namespace {
bool identifier(std::string_view key) {
    const auto letter = [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
    };
    if (key.empty() || !letter(key.front())) {
        return false;
    }
    for (unsigned char ch : key) {
        if (!letter(ch) && !(ch >= '0' && ch <= '9')) {
            return false;
        }
    }
    return true;
}

std::string format_path(const detail::PathNode* node) {
    std::vector<const detail::PathNode*> nodes;
    for (; node; node = node->prev) {
        nodes.push_back(node);
    }
    std::string path = "$";
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        if (const auto* key = std::get_if<std::string_view>(&(*it)->segment)) {
            if (identifier(*key)) {
                path += '.';
                path += *key;
            } else {
                path += '[';
                path += boost::json::serialize(boost::json::string(*key));
                path += ']';
            }
        } else {
            path += '[';
            path += std::to_string(std::get<std::size_t>((*it)->segment));
            path += ']';
        }
    }
    return path;
}
} // namespace

void detail::DecodeState::capture(boost::system::error_code code,
                                 std::string_view expected, std::string_view actual) {
    if (enabled && !error) {
        error.emplace(DecodeError{format_path(path), code, std::string(expected), std::string(actual)});
    }
}

std::string_view detail::actual_type(boost::json::kind kind) noexcept {
    switch (kind) {
    case boost::json::kind::null: return "null";
    case boost::json::kind::bool_: return "boolean";
    case boost::json::kind::int64: return "integer";
    case boost::json::kind::uint64: return "unsigned integer";
    case boost::json::kind::double_: return "number";
    case boost::json::kind::string: return "string";
    case boost::json::kind::array: return "array";
    case boost::json::kind::object: return "object";
    }
    return "value";
}

void detail::capture_conversion(DecodeState& state, boost::system::error_code code,
                               std::string_view expected, boost::json::kind actual) {
    // A custom converter may accept a different wire type than its C++ model.
    if (code == boost::json::error::not_string) {
        expected = "string";
    } else if (code == boost::json::error::not_object) {
        expected = "object";
    } else if (code == boost::json::error::not_array) {
        expected = "array";
    }
    state.capture(code, expected, actual_type(actual));
}

void detail::capture_size(DecodeState& state, std::size_t expected, std::size_t actual,
                          boost::json::kind kind) {
    if (state.enabled && !state.error) {
        const std::string prefix = kind == boost::json::kind::object ? "object of " : "array of ";
        const std::string_view suffix = kind == boost::json::kind::object ? " members" : " elements";
        state.capture(boost::json::error::size_mismatch,
            prefix + std::to_string(expected) + std::string(suffix),
            prefix + std::to_string(actual) + std::string(suffix));
    }
}

void detail::capture_child(DecodeState& state, PathSegment segment, boost::system::error_code code,
                          std::string_view expected, boost::json::kind actual) {
    if (state.enabled && !state.error) {
        PathScope scope(state, segment);
        capture_conversion(state, code, expected, actual);
    }
}

std::string DecodeError::message() const {
    if (actual == "missing") {
        return path + ": required field is missing (expected " + expected + ")";
    }
    return path + ": expected " + expected + ", got " + actual + " (" + code.message() + ")";
}
} // namespace aniparse::json
