#pragma once

#include "aniparse/utility/Expected.hpp"

namespace aniparse {
struct RequestError;

/**
 * @brief A value or an operation-specific error, without transport dependencies.
 * @tparam T The success value.
 * @tparam Error The error value; include Result.hpp for the default RequestError.
 */
template<class T, class Error = RequestError>
using Result = expected<T, Error>;
} // namespace aniparse
