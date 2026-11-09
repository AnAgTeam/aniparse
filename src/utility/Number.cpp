/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/utility/Number.hpp"

namespace aniparse::number::detail {
std::string_view trim_numeric_prefix(std::string_view text) noexcept {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'
        || text.front() == '\n' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    return text;
}
} // namespace aniparse::number::detail
