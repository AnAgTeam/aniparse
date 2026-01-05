#include "catch_amalgamated.hpp"
#include <aniparse/detail/BitsetLite.hpp>

using namespace aniparse::detail;

TEST_CASE("Bitset initialization from value") {
	constexpr BitsetLite<20> empty_bitset;

	BitsetLite<20> bitset1;
	BitsetLite<20> bitset2 = 0b100;

	BitsetLite<200> bitset3 = 0x9876553221ULL;
	BitsetLite<200> bitset4 = 0x1236137890ULL;

	REQUIRE(bitset1 == empty_bitset);
	REQUIRE(bitset2 != empty_bitset);

	REQUIRE(bitset1.to_ulong() == 0UL);
	REQUIRE(bitset2.to_ulong() == 0b100UL);

	REQUIRE(bitset3.to_ullong() == 0x9876553221ULL);
	REQUIRE(bitset4.to_ullong() == 0x1236137890ULL);

	REQUIRE(bitset3.to_ulong() == 0x76553221UL);
	REQUIRE(bitset4.to_ulong() == 0x36137890UL);

}

TEST_CASE("Bitset initialization from copy/move") {
	BitsetLite<20> bitset1 = 0x12361390UL;

	BitsetLite<20> bitset_copy = bitset1;
	BitsetLite<20> bitset_move = std::move(bitset1);

	REQUIRE(bitset_copy == bitset1);
	REQUIRE(bitset_move == bitset1);
}

TEST_CASE("Bitset value cutting") {
	BitsetLite<200> bitset1 = 0x1236137890ULL;
	BitsetLite<20> bitset2 = 0x12361890UL;

	REQUIRE(bitset1.to_ullong() == 0x1236137890ULL);

	REQUIRE(bitset2.to_ulong() == 0b1100001100010010000);
	REQUIRE(bitset2.to_ullong() == 0b1100001100010010000);
}

TEST_CASE("Bitset binary operations") {
	constexpr BitsetLite<200> empty_bitset;
	constexpr BitsetLite<200> expected_or = 0x9A76567AB1ULL;
	constexpr BitsetLite<200> expected_and = 0x1034543000ULL;
	constexpr BitsetLite<200> expected_xor = 0x8A42024AB1ULL;
	constexpr BitsetLite<64> expected_not = 0xFFFFFFEDCBA9876FULL;

	BitsetLite<200> bitset1 = 0x1234567890ULL;
	BitsetLite<200> bitset2 = 0x9876543221ULL;
	BitsetLite<64> bitset3 = 0x1234567890ULL;

	CHECK((bitset1 | bitset2) == expected_or);
	CHECK((bitset1 & bitset2) == expected_and);
	CHECK((bitset1 ^ bitset2) == expected_xor);
	CHECK((bitset1 ^ bitset1) == empty_bitset);
	CHECK(~bitset3 == expected_not);
}

TEST_CASE("Bitset out of range") {
	BitsetLite<20> bitset1;
	BitsetLite<200> bitset2;

	REQUIRE_NOTHROW(bitset1.set(19));
	REQUIRE_NOTHROW(bitset1.set(0));

	REQUIRE_THROWS_AS(bitset1.set(28), std::out_of_range);
	REQUIRE_THROWS_AS(bitset2.set(280), std::out_of_range);

	REQUIRE_NOTHROW(bitset1.reset(19));
	REQUIRE_NOTHROW(bitset1.reset(0));

	REQUIRE_THROWS_AS(bitset1.reset(33), std::out_of_range);
	REQUIRE_THROWS_AS(bitset2.reset(333), std::out_of_range);

	REQUIRE_NOTHROW(bitset1.test(19));
	REQUIRE_NOTHROW(bitset1.test(0));

	REQUIRE_THROWS_AS(bitset1.test(44), std::out_of_range);
	REQUIRE_THROWS_AS(bitset2.test(1233), std::out_of_range);
}

TEST_CASE("Bitset bit access") {
	BitsetLite<20> bitset1 = 0b10011;
	BitsetLite<200> bitset2 = 0x1234567890ULL;

	REQUIRE((bitset1[0] && bitset1[4] && !bitset1[3]));
	REQUIRE(!bitset1.test(3));
	REQUIRE((bitset2[20] && bitset2[33] && !bitset2[34] && !bitset2[3]));

	bitset1[3] = true;
	bitset1[0] = false;
	bitset2[3] = true;
	bitset2[34] = true;
	bitset2[33] = false;

	REQUIRE((!bitset1[0] && bitset1[3]));
	REQUIRE((!bitset1.test(0) && bitset1.test(3)));
	REQUIRE((bitset2[34] && bitset2[34] && !bitset2[33]));
}

TEST_CASE("Zero size bitset") {
	BitsetLite<0> bitset1;
}