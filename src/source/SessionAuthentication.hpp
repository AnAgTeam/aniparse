#pragma once
#include "aniparse/source/Authentication.hpp"
#include "aniparse/source/ParserConfig.hpp"

namespace aniparse::detail {
// Internal conversion between live request settings and portable credentials.
void import_auth(ParserConfig& config, const AuthState& state);
AuthState export_auth(const ParserConfig& config, const AuthKeys& keys);
} // namespace aniparse::detail
