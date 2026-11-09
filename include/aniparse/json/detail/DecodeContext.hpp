#pragma once

#include "aniparse/json/DecodeError.hpp"
#include <boost/json/kind.hpp>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

namespace aniparse::json::detail {

using PathSegment = std::variant<std::string_view, std::size_t>;

// These nodes only borrow their synchronous caller's frame. capture() copies
// the complete path before any of the frames unwind; no node escapes decode().
struct PathNode {
    const PathNode* prev;
    PathSegment segment;
};

struct DecodeState {
    const PathNode* path = nullptr;
    std::optional<DecodeError> error;
    bool enabled = true;

    void capture(boost::system::error_code code, std::string_view expected,
                 std::string_view actual);
};

struct DecodeContext {
    DecodeState* state;
};

template<class UserContext>
struct DecodeContextFor : DecodeContext {};

std::string_view actual_type(boost::json::kind kind) noexcept;
void capture_conversion(DecodeState& state, boost::system::error_code code,
                        std::string_view expected, boost::json::kind actual);
void capture_size(DecodeState& state, std::size_t expected, std::size_t actual,
                  boost::json::kind kind = boost::json::kind::array);
void capture_child(DecodeState& state, PathSegment segment, boost::system::error_code code,
                   std::string_view expected, boost::json::kind actual);

template<class T> struct IsTuple : std::false_type {};
template<class... T> struct IsTuple<std::tuple<T...>> : std::true_type {};

template<class Context>
DecodeState* decode_state(const Context& context) {
    if constexpr (std::is_base_of_v<DecodeContext, std::remove_cvref_t<Context>>) {
        return context.state;
    } else if constexpr (IsTuple<std::remove_cvref_t<Context>>::value) {
        return std::apply([](const auto&... parts) {
            DecodeState* result = nullptr;
            ((result = result ? result : decode_state(parts)), ...);
            return result;
        }, context);
    } else {
        return nullptr;
    }
}

class PathScope {
public:
    PathScope(DecodeState& state, PathSegment segment)
        : state_(state), node_{state.path, segment} {
        state_.path = &node_;
    }
    ~PathScope() { state_.path = node_.prev; }
    PathScope(const PathScope&) = delete;
    PathScope& operator=(const PathScope&) = delete;
private:
    DecodeState& state_;
    PathNode node_;
};

class QuietDiagnostics {
public:
    explicit QuietDiagnostics(DecodeState* state)
        : state_(state), enabled_(state && state->enabled) {
        if (state_) {
            state_->enabled = false;
        }
    }
    ~QuietDiagnostics() {
        if (state_) {
            state_->enabled = enabled_;
        }
    }
    QuietDiagnostics(const QuietDiagnostics&) = delete;
    QuietDiagnostics& operator=(const QuietDiagnostics&) = delete;
private:
    DecodeState* state_;
    bool enabled_;
};
} // namespace aniparse::json::detail
