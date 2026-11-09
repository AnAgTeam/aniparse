#pragma once

#include <aniparse/utility/Tolerant.hpp>

#include <boost/json.hpp>
#include <tuple>
#include <type_traits>

namespace aniparse::json {

/**
 * @brief Propagate invalid Tolerant values during Boost.JSON model conversion.
 * @note Pass to boost::json::try_value_to<Model>(document, json::Strict{}),
 *       alone or in a tuple of conversion contexts. Missing fields and explicit
 *       null remain allowed. Ordinary fields keep Boost.JSON's conversion rules.
 *       Use the same models and converters in production and fixture tests.
 */
struct Strict {};

namespace detail {

template<class Context>
inline constexpr bool strict_conversion = std::is_same_v<std::remove_cvref_t<Context>, Strict>;

template<class... Contexts>
inline constexpr bool strict_conversion<std::tuple<Contexts...>> =
    (strict_conversion<std::remove_cvref_t<Contexts>> || ...);

} // namespace detail
} // namespace aniparse::json

namespace aniparse {

/**
 * @brief Decode an optional value, discarding conversion errors unless strict.
 * @tparam T The owned value type.
 * @tparam Context The context selected by Boost.JSON.
 * @tparam FullContext All contexts passed to the enclosing model conversion.
 * @param input The JSON value; explicit null produces an empty wrapper.
 * @param context The full context, forwarded unchanged to nested conversions.
 * @return The decoded value, an empty wrapper, or the inner error in strict mode.
 * @note Missing described members are optional because Tolerant supplies value()
 *       and reset(). No exceptions are caught here; allocation failures propagate.
 */
template<class T, class Context, class FullContext>
boost::system::result<Tolerant<T>> tag_invoke(
    boost::json::try_value_to_tag<Tolerant<T>>, const boost::json::value& input,
    const Context&, const FullContext& context) {
    if (input.is_null()) {
        return Tolerant<T>{};
    }
    auto converted = boost::json::try_value_to<T>(input, context);
    if (converted) {
        return Tolerant<T>(std::move(*converted));
    }
    if constexpr (json::detail::strict_conversion<FullContext>) {
        return {boost::system::in_place_error, converted.error()};
    }
    return Tolerant<T>{};
}

} // namespace aniparse
