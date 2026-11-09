#pragma once
#include "aniparse/source/ParserConfig.hpp"
#include "aniparse/source/SessionConfig.hpp"
#include "aniparse/source/Authentication.hpp"

namespace aniparse::detail {

// Owned metadata captured once when a profile creates a session. It does not
// borrow the parser/profile and is never stored in user configuration.
struct ProfileBinding {
    std::string parser_id;
    std::string profile_id;
    std::vector<std::string> mirrors;
    std::vector<std::string_view> mirror_views;
    std::string canonical_base_url;
    ParserConfig defaults;
    AuthKeys auth_keys;
};

// Settings publication is small: changing a URL shares the request config/jar.
struct SessionState {
    std::shared_ptr<const ProfileBinding> binding;
    std::shared_ptr<const ParserConfig> config;
    SessionConfig settings;
};

} // namespace aniparse::detail
