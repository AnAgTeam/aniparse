/// aniparse core build metadata.
#pragma once

#include <string_view>

namespace aniparse {

/**
 * @brief Return the semantic version of this linked aniparse core.
 * @return A non-owning view of a process-lifetime, null-terminated semantic-version string.
 * @note The version originates from the CMake @c project(aniparse VERSION ...) declaration. The
 *       returned view remains valid until process termination.
 */
[[nodiscard]] std::string_view version() noexcept;

} // namespace aniparse
