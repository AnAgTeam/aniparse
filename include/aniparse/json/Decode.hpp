#pragma once

#include "aniparse/json/detail/DecodeValue.hpp"
#include "aniparse/utility/ResultFwd.hpp"
#include <stdexcept>

namespace aniparse::json {

namespace detail {
template<class Context>
using DecodeErrorFor = std::conditional_t<has_context<Diagnostics, std::remove_cvref_t<Context>>,
    DecodeError, boost::system::error_code>;
}

/**
 * @brief Convert a JSON value using the selected validation and error policy.
 * @tparam T Destination model or value type.
 * @tparam Context Boost.JSON contexts; defaults to the build's DefaultContext.
 * @param input JSON borrowed for this synchronous call.
 * @param context User conversions/settings, forwarded to nested conversions.
 * @return Decoded value or a Boost error code; with Diagnostics, a DecodeError.
 * @note Without Diagnostics the diagnostic traversal is not instantiated.
 *       An explicit context replaces the default, including the build policy.
 *       Diagnostics allocate only on an unsuppressed failure. Tolerant failures
 *       and unsuccessful variant alternatives do not allocate diagnostics.
 *       Custom converters take precedence and are opaque unless they forward
 *       the full context; failures are located at their conversion boundary.
 *       Views retain their original borrowing contract; this function does not
 *       extend input lifetime. Container destinations must own their storage.
 * @throws std::bad_alloc If model or error storage cannot be allocated.
 */
template<class T, class Context = DefaultContext>
Result<std::remove_cvref_t<T>, detail::DecodeErrorFor<Context>> decode(
    const boost::json::value& input, const Context& context = {}) {
    if constexpr (detail::has_context<Diagnostics, std::remove_cvref_t<Context>>) {
        detail::DecodeState state;
        detail::DecodeContextFor<Context> diagnostic{{&state}};
        // User converters keep precedence. The tuple's type stays fixed throughout
        // recursion: field names and array indices are values, never template args.
        const auto full = std::tie(context, diagnostic);
        auto result = detail::read<std::remove_cvref_t<T>>(input, state, full);
        if (!result) {
            return unexpected(std::move(*state.error));
        }
        return std::move(*result);
    } else {
        auto result = boost::json::try_value_to<std::remove_cvref_t<T>>(input, context);
        if (!result) {
            return unexpected(result.error());
        }
        return std::move(*result);
    }
}

/**
 * @brief Diagnose a failed conversion in a separate pass over the parsed JSON.
 * @tparam T The destination type of the failed Boost.JSON conversion.
 * @tparam Context The same conversion context used in the first pass.
 * @param input Parsed JSON, borrowed only during this synchronous call.
 * @param context Settings forwarded unchanged, including the Strict policy.
 * @return An owned diagnostic with the failing JSON path.
 * @pre Conversion to T has failed with this input and context. Converters must
 *      produce repeatable results and tolerate being called again.
 * @note Call only in the error branch of boost::json::try_value_to. Successful
 *       conversions then use ordinary Boost.JSON without a diagnostic context.
 *       The second pass can construct temporary models; it does not parse JSON
 *       text again. Use decode() for converters that cannot be repeated.
 * @throws std::logic_error If the failure cannot be reproduced.
 * @throws std::bad_alloc If model or diagnostic storage cannot be allocated.
 */
template<class T, class Context = std::nullptr_t>
DecodeError decode_error(const boost::json::value& input, const Context& context = {}) {
    // A replay must preserve the original policy, independently of build defaults.
    auto result = decode<T>(input, std::tuple<const Context&, Diagnostics>{context, {}});
    if (result) {
        throw std::logic_error("JSON conversion failure could not be reproduced");
    }
    return std::move(result.error());
}
} // namespace aniparse::json
