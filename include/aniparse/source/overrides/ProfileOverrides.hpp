/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include <optional>
#include <string>
#include <vector>

namespace aniparse {

/**
 * @brief Independent remote replacements for one request profile's origins.
 *
 * An absent channel preserves the compiled fallback. An explicitly empty mirror
 * list remains distinguishable from an absent mirror override.
 */
struct ProfileOverrides {
    std::optional<std::vector<std::string>> mirrors; ///< Replacement fetch origins, when supplied.
    std::optional<std::string> canonical_base_url; ///< Replacement public frontend origin.
};

} // namespace aniparse
