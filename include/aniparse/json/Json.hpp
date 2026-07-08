/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/Attributes.hpp"

#include <boost/json.hpp>

#include <cstdint>
#include <string>
#include <string_view>

/**
 * @file
 * Safe read-only navigation over a parsed boost::json value: typed field
 * lookups that yield a value-or-fallback instead of throwing, so a parser can
 * map an API payload onto its model without hand-writing a presence and type
 * check at every step. This is the JSON counterpart to the CSS-selector
 * helpers for HTML — the walking half of the JSON layer, paired with the
 * parsing half in aniparse/html/JSParser.hpp.
 *
 * Every accessor has two overloads: one takes the object by reference, one
 * takes a const object pointer. The pointer overload short-circuits on nullptr
 * to the same "absent" fallback, so lookups chain without an intermediate
 * null check — object_field(root.if_object(), "data") feeds straight into the
 * next accessor even when the parent is missing.
 *
 * Lifetime: object_field/array_field return a pointer that borrows into the
 * root value, valid only while that root value is alive — the same contract as
 * boost::json::value::if_object/if_contains. Do not walk from the result of a
 * temporary (e.g. object_field(parse(body).as_object(), ...)); bind the root
 * value to a named variable first. The scalar accessors (str/integer/number/
 * boolean) return by value and carry no such dependency.
 */

namespace aniparse::json {

/**
 * @brief The object-typed value at a key.
 * @param object The object to read from.
 * @param key    The field name to look up.
 * @return A pointer to the nested object, or nullptr if the key is absent or
 *         the value is not an object.
 */
inline const boost::json::object* object_field(const boost::json::object& object ANIPARSE_LIFETIMEBOUND,
                                               std::string_view key) {
	const boost::json::value* value = object.if_contains(key);
	return value ? value->if_object() : nullptr;
}

/**
 * @brief The array-typed value at a key.
 * @param object The object to read from.
 * @param key    The field name to look up.
 * @return A pointer to the array, or nullptr if the key is absent or the value
 *         is not an array.
 */
inline const boost::json::array* array_field(const boost::json::object& object ANIPARSE_LIFETIMEBOUND,
                                             std::string_view key) {
	const boost::json::value* value = object.if_contains(key);
	return value ? value->if_array() : nullptr;
}

/**
 * @brief The string value at a key. Handles embedded NUL characters.
 * @param object The object to read from.
 * @param key    The field name to look up.
 * @return The string, or empty if the key is absent or the value is not a
 *         string.
 */
inline std::string str(const boost::json::object& object, std::string_view key) {
	const boost::json::value* value = object.if_contains(key);
	return value && value->is_string() ? boost::json::value_to<std::string>(*value)
	                                   : std::string{};
}

/**
 * @brief The integer value at a key. Coerces a number stored as unsigned or
 *        double (the double is truncated toward zero).
 * @param object The object to read from.
 * @param key    The field name to look up.
 * @return The integer, or 0 if the key is absent or the value is not numeric.
 */
inline std::int64_t integer(const boost::json::object& object, std::string_view key) {
	const boost::json::value* value = object.if_contains(key);
	if (!value) {
		return 0;
	}
	if (value->is_int64())  return value->as_int64();
	if (value->is_uint64()) return static_cast<std::int64_t>(value->as_uint64());
	if (value->is_double()) return static_cast<std::int64_t>(value->as_double());
	return 0;
}

/**
 * @brief The floating-point value at a key. Coerces a number stored as a signed
 *        or unsigned integer.
 * @param object The object to read from.
 * @param key    The field name to look up.
 * @return The value, or 0.0 if the key is absent or the value is not numeric.
 */
inline double number(const boost::json::object& object, std::string_view key) {
	const boost::json::value* value = object.if_contains(key);
	if (!value) {
		return 0.0;
	}
	if (value->is_double()) return value->as_double();
	if (value->is_int64())  return static_cast<double>(value->as_int64());
	if (value->is_uint64()) return static_cast<double>(value->as_uint64());
	return 0.0;
}

/**
 * @brief The boolean value at a key.
 * @param object The object to read from.
 * @param key    The field name to look up.
 * @return The boolean, or false if the key is absent or the value is not a bool.
 */
inline bool boolean(const boost::json::object& object, std::string_view key) {
	const boost::json::value* value = object.if_contains(key);
	return value && value->is_bool() ? value->as_bool() : false;
}

/**
 * @brief Pointer overload of object_field.
 * @param object The object to read from, or nullptr.
 * @param key    The field name to look up.
 * @return object_field(*object, key), or nullptr when @p object is nullptr.
 */
inline const boost::json::object* object_field(const boost::json::object* object,
                                               std::string_view key) {
	return object ? object_field(*object, key) : nullptr;
}

/**
 * @brief Pointer overload of array_field.
 * @param object The object to read from, or nullptr.
 * @param key    The field name to look up.
 * @return array_field(*object, key), or nullptr when @p object is nullptr.
 */
inline const boost::json::array* array_field(const boost::json::object* object,
                                             std::string_view key) {
	return object ? array_field(*object, key) : nullptr;
}

/**
 * @brief Pointer overload of str.
 * @param object The object to read from, or nullptr.
 * @param key    The field name to look up.
 * @return str(*object, key), or empty when @p object is nullptr.
 */
inline std::string str(const boost::json::object* object, std::string_view key) {
	return object ? str(*object, key) : std::string{};
}

/**
 * @brief Pointer overload of integer.
 * @param object The object to read from, or nullptr.
 * @param key    The field name to look up.
 * @return integer(*object, key), or 0 when @p object is nullptr.
 */
inline std::int64_t integer(const boost::json::object* object, std::string_view key) {
	return object ? integer(*object, key) : 0;
}

/**
 * @brief Pointer overload of number.
 * @param object The object to read from, or nullptr.
 * @param key    The field name to look up.
 * @return number(*object, key), or 0.0 when @p object is nullptr.
 */
inline double number(const boost::json::object* object, std::string_view key) {
	return object ? number(*object, key) : 0.0;
}

/**
 * @brief Pointer overload of boolean.
 * @param object The object to read from, or nullptr.
 * @param key    The field name to look up.
 * @return boolean(*object, key), or false when @p object is nullptr.
 */
inline bool boolean(const boost::json::object* object, std::string_view key) {
	return object ? boolean(*object, key) : false;
}

} // namespace aniparse::json
