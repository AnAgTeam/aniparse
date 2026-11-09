#pragma once
#include <aniparse/json/Decode.hpp>

namespace decode_test {
// Diagnostic regression cases explicitly choose their policy so they exercise
// the same behavior with ANIPARSE_JSON_CHECKS both enabled and disabled.
template<class T, class Context = std::nullptr_t>
auto diagnose(const boost::json::value& input, const Context& context = {}) {
    return aniparse::json::decode<T>(input,
        std::tuple<const Context&, aniparse::json::Diagnostics>{context, {}});
}
}
