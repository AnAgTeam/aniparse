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

TEST_CASE("StrongBitset::test is true iff this contains every bit of the argument") {
	const TestBitset sugg  = TestBitset::make_bit(0);
	const TestBitset adult = TestBitset::make_bit(1);
	const TestBitset both  = sugg | adult;
	const TestBitset none;

	// {sugg} does not contain {sugg, adult}; the fuller set contains the subset.
	CHECK_FALSE(sugg.test(both));
	CHECK(both.test(both));
	CHECK(both.test(sugg));

	// Single-bit membership: "is the flag set".
	CHECK(both.test(adult));
	CHECK_FALSE(sugg.test(adult));

	// The empty set contains no flag, but every set contains the empty set.
	CHECK_FALSE(none.test(sugg));
	CHECK(both.test(none));
	CHECK(none.test(none));
}

TEST_CASE("Unable to use StrongBitset with arbitrary") {
	REQUIRE_FALSE(can_implicit_init_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_or_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_and_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_xor_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_set_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_get_with_arbitrary<TestBitset>);
}