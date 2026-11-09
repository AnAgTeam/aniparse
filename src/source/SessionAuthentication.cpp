#include "SessionAuthentication.hpp"
#include <algorithm>

namespace aniparse::detail {
void import_auth(ParserConfig& config, const AuthState& state) {
    if (config.cookie_jar) {
        for (const auto& cookie : state.cookies) config.cookie_jar->set_cookie(cookie);
    }
    for (const auto& [name, value] : state.headers) config.headers.erase(name);
    for (const auto& [name, value] : state.headers) config.headers.append(name, value);
    for (const auto& [name, value] : state.url_params) config.url_params[name] = value;
}

AuthState export_auth(const ParserConfig& config, const AuthKeys& keys) {
    AuthState state;
    if (config.cookie_jar) {
        for (const auto& cookie : config.cookie_jar->cookies()) {
            if (std::find(keys.cookies.begin(), keys.cookies.end(), cookie.name) != keys.cookies.end()) {
                state.cookies.push_back(cookie);
            }
        }
    }
    for (const auto& name : keys.headers) {
        for (auto value : config.headers.get_all(name)) state.headers.append(name, std::string(value));
    }
    for (const auto& name : keys.url_params) {
        if (auto it = config.url_params.find(name); it != config.url_params.end()) state.url_params.emplace(*it);
    }
    return state;
}
} // namespace aniparse::detail
