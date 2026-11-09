#pragma once
#include "aniparse/net/Headers.hpp"
#include <map>
#include <optional>
#include <string>

namespace aniparse {

/**
 * @brief Persistable user choices, independent of credentials and profile defaults.
 * A missing URL selects the current remote override or the built-in default.
 * An explicit URL remains selected across override updates.
 */
struct SessionConfig {
    std::optional<std::string> base_url; ///< Explicit fetch base, including an optional path.
    std::optional<std::string> canonical_base_url; ///< Independent frontend origin override.
    Headers headers; ///< User request defaults; credential fields belong to authentication.
    std::map<std::string, std::string> url_params; ///< Non-credential user query defaults.
    friend bool operator==(const SessionConfig&, const SessionConfig&) = default;
};

} // namespace aniparse
