/* Copyright (C) 2026 Toilettrauma */
#pragma once
#include <memory>
#include <string>

namespace aniparse {

/**
 * @brief A recommended work and the source's explanation of its similarity.
 * The reason belongs to this recommendation, not to the recommended work's
 * metadata or serialized identity. Consumers decide whether to retain it.
 * @tparam Getter The recommended work's getter interface.
 */
template <typename Getter>
struct Recommendation {
    /// Independently owned getter for opening the recommended work.
    std::unique_ptr<Getter> getter;
    /// Source-provided free text; empty when no explanation is supplied.
    std::string reason;
};

} // namespace aniparse
