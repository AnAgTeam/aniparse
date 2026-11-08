#pragma once
#include <variant>

namespace aniparse {

/// The interaction has not completed yet; this is not a failed check.
struct InteractionPending {};

/**
 * @brief A completed interaction carrying its operation-specific result.
 * @tparam T Owned result type; use std::monostate when no payload is needed.
 */
template<class T>
struct InteractionCompleted {
    T value; ///< Result for the caller to accept and apply explicitly.
};

/**
 * @brief Successful check states, independent of transport and authentication.
 * @tparam T Completion payload, which may be move-only.
 * @note Check failures belong in the enclosing Result error channel.
 */
template<class T>
using InteractionState = std::variant<InteractionPending, InteractionCompleted<T>>;

} // namespace aniparse
