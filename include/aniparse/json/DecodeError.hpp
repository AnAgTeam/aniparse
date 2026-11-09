#pragma once

#include <boost/system/error_code.hpp>
#include <string>

namespace aniparse::json {

/**
 * @brief An owned JSON conversion diagnostic independent of the input document.
 * @note Paths start with $, use .name for simple keys, ["key"] for other keys,
 *       and [index] for every JSON array, including tuples and pairs.
 */
struct DecodeError {
    std::string path; ///< Location in the input JSON, not a C++ type/member path.
    boost::system::error_code code; ///< Original Boost.JSON or custom error code.
    std::string expected; ///< Expected JSON shape or numeric representation.
    std::string actual; ///< Actual shape, or "missing" for an absent member.

    /**
     * @brief Format the owned diagnostic for a log or response error.
     * @return A message containing the path, mismatch and underlying cause.
     */
    std::string message() const;
};
} // namespace aniparse::json
