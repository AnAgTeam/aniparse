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

TEST_CASE("StrongBitset::has is true iff this contains every bit of the argument") {
	const TestBitset sugg  = TestBitset::make_bit(0);
	const TestBitset adult = TestBitset::make_bit(1);
	const TestBitset both  = sugg | adult;
	const TestBitset none;

	// {sugg} does not contain {sugg, adult}; the fuller set contains the subset.
	CHECK_FALSE(sugg.has(both));
	CHECK(both.has(both));
	CHECK(both.has(sugg));

	// Single-bit membership: "is the flag set".
	CHECK(both.has(adult));
	CHECK_FALSE(sugg.has(adult));

	// The empty set contains no flag, but every set contains the empty set.
	CHECK_FALSE(none.has(sugg));
	CHECK(both.has(none));
	CHECK(none.has(none));
}

TEST_CASE("Unable to use StrongBitset with arbitrary") {
	REQUIRE_FALSE(can_implicit_init_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_or_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_and_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_binary_xor_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_set_with_arbitrary<TestBitset>);
	REQUIRE_FALSE(can_get_with_arbitrary<TestBitset>);
}
#include <aniparse/utility/FlagsBitfield.hpp>
#include <stdexcept>

namespace {
using ConstexprFlags = aniparse::FlagsBitfield<8, struct ConstexprFlagsTag>;

constexpr bool flags_are_constexpr() {
    auto flags = ConstexprFlags::make_bit(0);
    flags[ConstexprFlags::Bit(1)] = true;
    auto copy = flags;
    copy = flags;
    copy = ConstexprFlags::make_bit(0) | ConstexprFlags::make_bit(1);
    if (!copy.has(flags) || copy.all() || copy.none()) return false;
    copy.set();
    if (!copy.all()) return false;
    copy.reset();
    return copy.none() && !copy.any();
}
static_assert(flags_are_constexpr());

// A throwing backing store exposes accidental unconditional noexcept in the wrapper.
struct ThrowingStorage : std::bitset<8> {
    ThrowingStorage() = default;
    ThrowingStorage(const ThrowingStorage& other) : std::bitset<8>(other) {
        throw std::runtime_error("copy");
    }
    ThrowingStorage(ThrowingStorage&&) noexcept = default;
    ThrowingStorage& operator|=(const ThrowingStorage&) & { throw std::runtime_error("or"); }
    ThrowingStorage& operator&=(const ThrowingStorage&) & { throw std::runtime_error("and"); }
    ThrowingStorage& operator^=(const ThrowingStorage&) & { throw std::runtime_error("xor"); }
};
using ThrowingFlags = StrongBitset<ThrowingStorage, struct ThrowingFlagsTag>;
static_assert(!noexcept(std::declval<ThrowingFlags&>() |= std::declval<const ThrowingFlags&>()));
static_assert(!noexcept(std::declval<ThrowingFlags&>() &= std::declval<const ThrowingFlags&>()));
static_assert(!noexcept(std::declval<ThrowingFlags&>() ^= std::declval<const ThrowingFlags&>()));
static_assert(!noexcept(std::declval<ThrowingFlags>() | std::declval<ThrowingFlags>()));
static_assert(!noexcept(std::declval<ThrowingFlags>() & std::declval<ThrowingFlags>()));
static_assert(!noexcept(std::declval<ThrowingFlags>() ^ std::declval<ThrowingFlags>()));
static_assert(!noexcept(static_cast<ThrowingStorage>(std::declval<ThrowingFlags&>())));
static_assert(!noexcept(TestBitset::make_bit(0)));
}

TEST_CASE("StrongBitset binary operators accept every operand value category", "[bitset]") {
    const TestBitset left{std::bitset<20>{0b101}};
    const TestBitset right{std::bitset<20>{0b110}};
    const TestBitset united{std::bitset<20>{0b111}};
    const TestBitset intersected{std::bitset<20>{0b100}};
    const TestBitset exclusive{std::bitset<20>{0b011}};
    CHECK((left | right) == united);
    CHECK((TestBitset{left} | right) == united);
    CHECK((left | TestBitset{right}) == united);
    CHECK((TestBitset{left} | TestBitset{right}) == united);
    CHECK((left & right) == intersected);
    CHECK((TestBitset{left} & right) == intersected);
    CHECK((left & TestBitset{right}) == intersected);
    CHECK((TestBitset{left} & TestBitset{right}) == intersected);
    CHECK((left ^ right) == exclusive);
    CHECK((TestBitset{left} ^ right) == exclusive);
    CHECK((left ^ TestBitset{right}) == exclusive);
    CHECK((TestBitset{left} ^ TestBitset{right}) == exclusive);
    CHECK((left | left) == left);
    CHECK((left & left) == left);
    CHECK((left ^ left).none());
    CHECK((~left & left).none());
}

TEST_CASE("StrongBitset propagates backing-store exceptions", "[bitset]") {
    CHECK_THROWS_AS(TestBitset::make_bit(20), std::out_of_range);
    ThrowingFlags left;
    const ThrowingFlags right;
    CHECK_THROWS_AS(static_cast<ThrowingStorage>(left), std::runtime_error);
    CHECK_THROWS_AS((left | right), std::runtime_error);
    CHECK_THROWS_AS((left & right), std::runtime_error);
    CHECK_THROWS_AS((left ^ right), std::runtime_error);
    CHECK_THROWS_AS((ThrowingFlags{} | ThrowingFlags{}), std::runtime_error);
    CHECK_THROWS_AS((ThrowingFlags{} & ThrowingFlags{}), std::runtime_error);
    CHECK_THROWS_AS((ThrowingFlags{} ^ ThrowingFlags{}), std::runtime_error);
}
