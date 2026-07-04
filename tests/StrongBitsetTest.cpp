/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <bitset>
#include <aniparse/detail/StrongBitset.hpp>

using namespace aniparse::detail;

using TestBitset = StrongBitset<std::bitset<20>, struct TestBitsetTag>;

template<typename T>
constexpr void test_implicit(T) {

}

template<typename T>
constexpr bool can_implicit_init_with_arbitrary = requires {
	{ test_implicit<T>(1ULL) };
};

template<typename T>
constexpr bool can_binary_or_with_arbitrary = requires (T a) {
	{ a |= 1ULL };
};

template<typename T>
constexpr bool can_binary_and_with_arbitrary = requires (T a) {
	{ a &= 1ULL };
};

template<typename T>
constexpr bool can_binary_xor_with_arbitrary = requires (T a) {
	{ a ^= 1ULL };
};

template<typename T>
constexpr bool can_set_with_arbitrary = requires (T a) {
	{ a[1ULL] = true };
};

template<typename T>
constexpr bool can_get_with_arbitrary = requires (T a) {
	{ a[1ULL] };
};

TEST_CASE("Unable to use StrongBitset with arbitrary") {
	REQUIRE_FALSE(can_implicit_init_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_or_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_and_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_xor_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_set_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_get_with_arbitrary<TestBitset>);
}