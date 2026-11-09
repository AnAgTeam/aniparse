#pragma once

#include "aniparse/json/Conversion.hpp"
#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <array>
#include <iterator>
#include <ranges>
#include <tuple>
#include <variant>

namespace aniparse::json::detail {

// Boost has no public hook for visiting generic conversions. Reuse its category,
// container and variant helpers here instead of maintaining different rules.
// Compatibility tests exercise this boundary when the Boost dependency changes.
namespace boost_conversion = boost::json::detail;

template<class T>
using ConversionCategory = boost_conversion::generic_conversion_category<T>;

template<class T, class Category>
inline constexpr bool converts_as = std::is_same_v<ConversionCategory<T>, Category>;

template<class T>
inline constexpr bool native_json = std::is_same_v<T, boost::json::value>
    || std::is_same_v<T, boost::json::array> || std::is_same_v<T, boost::json::object>
    || std::is_same_v<T, boost::json::string>;

template<class T>
inline constexpr bool traced_composite = !native_json<T>
    && (converts_as<T, boost_conversion::described_class_conversion_tag>
        || converts_as<T, boost_conversion::map_like_conversion_tag>
        || converts_as<T, boost_conversion::sequence_conversion_tag>
        || converts_as<T, boost_conversion::tuple_conversion_tag>
        || converts_as<T, boost_conversion::optional_conversion_tag>
        || converts_as<T, boost_conversion::variant_conversion_tag>);

template<class T, class Context>
constexpr bool has_custom_conversion() {
    using Try = boost::json::try_value_to_tag<T>;
    using Throw = boost::json::value_to_tag<T>;
    if constexpr (requires(const boost::json::value& value) { tag_invoke(Try{}, value); }
                  || requires(const boost::json::value& value) { tag_invoke(Throw{}, value); }) {
        return true;
    } else if constexpr (IsTuple<std::remove_cvref_t<Context>>::value) {
        return []<class... Parts>(const std::tuple<Parts...>*) {
            return (has_custom_conversion<T, std::remove_cvref_t<Parts>>() || ...);
        }(static_cast<const std::remove_cvref_t<Context>*>(nullptr));
    } else {
        return requires(const boost::json::value& value, const Context& context) {
            tag_invoke(Try{}, value, context, context);
        } || requires(const boost::json::value& value, const Context& context) {
            tag_invoke(Try{}, value, context);
        } || requires(const boost::json::value& value, const Context& context) {
            tag_invoke(Throw{}, value, context, context);
        } || requires(const boost::json::value& value, const Context& context) {
            tag_invoke(Throw{}, value, context);
        };
    }
}

// Declared before read(): ADL must see this customization when Boost detects
// conversion categories, including recursive instantiations of the same model.
template<class T, class UserContext, class FullContext>
    requires (traced_composite<T> && !has_custom_conversion<T, UserContext>())
boost::system::result<T> tag_invoke(boost::json::try_value_to_tag<T>,
    const boost::json::value& input, const DecodeContextFor<UserContext>& context, const FullContext& full);

template<class T>
constexpr std::string_view expected_type() {
    if constexpr (converts_as<T, boost_conversion::optional_conversion_tag>) {
        return expected_type<boost_conversion::value_result_type<T>>();
    } else if constexpr (std::is_same_v<T, bool>) {
        return "boolean";
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_unsigned_v<T>) {
            return "unsigned integer";
        } else {
            return "signed integer";
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        return "number";
    } else if constexpr (boost::json::is_null_like<T>::value) {
        return "null";
    } else if constexpr (converts_as<T, boost_conversion::string_like_conversion_tag>
                         || converts_as<T, boost_conversion::path_conversion_tag>
                         || converts_as<T, boost_conversion::described_enum_conversion_tag>) {
        return "string";
    } else if constexpr (converts_as<T, boost_conversion::described_class_conversion_tag>
                         || converts_as<T, boost_conversion::map_like_conversion_tag>) {
        return "object";
    } else if constexpr (converts_as<T, boost_conversion::sequence_conversion_tag>
                         || converts_as<T, boost_conversion::tuple_conversion_tag>) {
        return "array";
    } else if constexpr (converts_as<T, boost_conversion::variant_conversion_tag>) {
        return "a matching variant alternative";
    } else {
        return "a convertible value";
    }
}

template<class T, class Context>
boost::system::result<T> read(const boost::json::value& input,
                             DecodeState& state, const Context& context) {
    auto result = boost::json::try_value_to<T>(input, context);
    if (!result && state.enabled && !state.error) {
        capture_conversion(state, result.error(), expected_type<T>(), input.kind());
    }
    return result;
}

template<class T, class Context>
boost::system::result<T> read_child(const boost::json::value& input, PathSegment segment,
                                   DecodeState& state, const Context& context) {
    // Built-in leaves cannot descend further. Keep their path node entirely on
    // the failure branch, particularly in loops over homogeneous scalar arrays.
    if constexpr ((std::is_arithmetic_v<T> || boost::json::is_string_like<T>::value)
                  && !has_custom_conversion<T, Context>()) {
        auto result = boost::json::try_value_to<T>(input, context);
        if (!result) [[unlikely]] {
            capture_child(state, segment, result.error(), expected_type<T>(), input.kind());
        }
        return result;
    }
    PathScope scope(state, segment);
    return read<T>(input, state, context);
}

template<class T, class Context>
boost::system::result<T> read_object(const boost::json::value& input,
                                    DecodeState& state, const Context& context) {
    static_assert(boost_conversion::uniquely_named_members<T>::value,
        "The type has several described members with the same name.");
    const auto* object = input.if_object();
    if (!object) {
        return {boost::system::in_place_error, boost::json::error::not_object};
    }
    T result{};
    boost::system::error_code error;
    using Members = boost_conversion::described_members<T>;
    boost::mp11::mp_for_each<Members>([&](auto member) {
        if (error) {
            return;
        }
        using Member = std::remove_cvref_t<decltype(result.*member.pointer)>;
        const auto* value = object->if_contains(member.name);
        if (!value) {
            if constexpr (!boost::json::is_optional_like<Member>::value) {
                error = boost::json::error::size_mismatch;
                if (state.enabled) {
                    PathScope scope(state, std::string_view(member.name));
                    state.capture(error, expected_type<Member>(), "missing");
                }
            }
            return;
        }
        auto decoded = read_child<Member>(*value, std::string_view(member.name), state, context);
        if (decoded) {
            result.*member.pointer = std::move(*decoded);
        } else {
            error = decoded.error();
        }
    });
    if (error) {
        return {boost::system::in_place_error, error};
    }
    return result;
}

template<class T>
boost::json::error reserve_container(T& result, std::size_t size, DecodeState& state) {
    const auto error = boost_conversion::try_reserve(
        result, size, boost_conversion::reserve_implementation<T>{});
    if constexpr (boost::json::is_tuple_like<T>::value) {
        if (error != boost::json::error{}) {
            constexpr auto kind = converts_as<T, boost_conversion::map_like_conversion_tag>
                ? boost::json::kind::object : boost::json::kind::array;
            capture_size(state, std::tuple_size_v<T>, size, kind);
        }
    }
    return error;
}

template<class T, class Context>
boost::system::result<T> read_sequence(const boost::json::value& input,
                                      DecodeState& state, const Context& context) {
    static_assert(!std::ranges::view<T>, "JSON models must own their containers; decode into storage before creating a view");
    const auto* array = input.if_array();
    if (!array) {
        return {boost::system::in_place_error, boost::json::error::not_array};
    }
    T result;
    if (auto error = reserve_container(result, array->size(), state); error != boost::json::error{}) {
        return {boost::system::in_place_error, error};
    }
    auto output = boost_conversion::inserter(result, boost_conversion::inserter_implementation<T>{});
    using Element = boost_conversion::value_type<T>;
    for (std::size_t index = 0; index < array->size(); ++index) {
        auto value = read_child<Element>((*array)[index], index, state, context);
        if (!value) {
            return {boost::system::in_place_error, value.error()};
        }
        *output++ = std::move(*value);
    }
    return result;
}

template<class T, class Context>
boost::system::result<T> read_map(const boost::json::value& input,
                                 DecodeState& state, const Context& context) {
    const auto* object = input.if_object();
    if (!object) {
        return {boost::system::in_place_error, boost::json::error::not_object};
    }
    static_assert(!std::ranges::view<T>, "JSON models must own their containers; decode into storage before creating a view");
    T result;
    if (auto error = reserve_container(result, object->size(), state); error != boost::json::error{}) {
        return {boost::system::in_place_error, error};
    }
    auto output = boost_conversion::inserter(result, boost_conversion::inserter_implementation<T>{});
    for (const auto& entry : *object) {
        auto value = read_child<boost_conversion::mapped_type<T>>(entry.value(),
            std::string_view(entry.key()), state, context);
        if (!value) {
            return {boost::system::in_place_error, value.error()};
        }
        *output++ = boost_conversion::value_type<T>{
            boost_conversion::key_type<T>(entry.key()), std::move(*value)};
    }
    return result;
}

template<class T, class Context>
boost::system::result<T> read_tuple_element(const boost::json::value& input, std::size_t index,
    DecodeState& state, const Context& context, boost::system::error_code& error) {
    if (error) {
        return {boost::system::in_place_error, error};
    }
    auto result = read_child<T>(input, index, state, context);
    error = result.error();
    return result;
}

template<class T, class Context, std::size_t... I>
boost::system::result<T> read_tuple(const boost::json::value& input, DecodeState& state,
                                   const Context& context, std::index_sequence<I...>) {
    const auto* array = input.if_array();
    if (!array) {
        return {boost::system::in_place_error, boost::json::error::not_array};
    }
    if (array->size() != sizeof...(I)) {
        capture_size(state, sizeof...(I), array->size());
        return {boost::system::in_place_error, boost::json::error::size_mismatch};
    }
    boost::system::error_code error;
    // Match Boost's decayed element types and argument evaluation order.
    auto values = std::make_tuple(read_tuple_element<std::decay_t<std::tuple_element_t<I, T>>>(
        (*array)[I], I, state, context, error)...);
    if (error) {
        return {boost::system::in_place_error, error};
    }
    return T{std::move(*std::get<I>(values))...};
}

template<class T, class Context>
boost::system::result<T> read_variant(const boost::json::value& input, DecodeState& state,
                                     const Context& context) {
    // Failed alternatives are speculative: no diagnostic allocations even if a
    // later alternative succeeds. An exhausted union is reported at this node.
    QuietDiagnostics quiet(&state);
    return boost_conversion::value_to_impl(boost_conversion::variant_conversion_tag{},
        boost::json::try_value_to_tag<T>{}, input, context);
}

template<class T, class UserContext, class FullContext>
    requires (traced_composite<T> && !has_custom_conversion<T, UserContext>())
boost::system::result<T> tag_invoke(boost::json::try_value_to_tag<T>,
    const boost::json::value& input, const DecodeContextFor<UserContext>& context, const FullContext& full) {
    auto& state = *context.state;
    if constexpr (converts_as<T, boost_conversion::variant_conversion_tag>) {
        return read_variant<T>(input, state, full);
    } else if constexpr (converts_as<T, boost_conversion::optional_conversion_tag>) {
        if (input.is_null()) {
            return {};
        }
        using Element = boost_conversion::value_result_type<T>;
        return read<Element>(input, state, full);
    } else if constexpr (converts_as<T, boost_conversion::map_like_conversion_tag>) {
        return read_map<T>(input, state, full);
    } else if constexpr (converts_as<T, boost_conversion::sequence_conversion_tag>) {
        return read_sequence<T>(input, state, full);
    } else if constexpr (converts_as<T, boost_conversion::tuple_conversion_tag>) {
        return read_tuple<T>(input, state, full, std::make_index_sequence<std::tuple_size_v<T>>{});
    } else {
        return read_object<T>(input, state, full);
    }
}
} // namespace aniparse::json::detail
