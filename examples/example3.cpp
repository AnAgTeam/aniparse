#include <aniparse/AniParse.hpp>

#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <span>
#include <optional>

#include <asyncnet/AsyncSession.hpp>
#include <coro/sync_wait.hpp>

#include <print>
#include <bitset>
#include <algorithm>

#include <aniparse/detail/BitsetLite.hpp>
#include <aniparse/detail/StrongBitset.hpp>
#include <aniparse/anime/Release.hpp>
#include <aniparse/Client.hpp>

#undef max
#undef min

using namespace aniparse;
using namespace aniparse::detail;

namespace concepts {
	template<typename T>
	concept EnumType = std::is_enum_v<T>;
}

template<typename Container, concepts::EnumType BitIndices>
constexpr bool can_use_integral_c = requires(StrongBitset<Container, BitIndices> bitset) {
	bitset.test(1ULL);
};

// for test
namespace std {
#ifndef __cpp_lib_print
	template<typename ... T>
	void println(T&& ... args) {

	}
#endif
}

void test_flags() {
	enum class MyFlags {
		Simple = 0,
		Hard = 1,
		Flag3 = 2,
		Flag4 = 3,
	};

	enum class MyFlags2 {
		Test1 = 0
	};

	using MyFlagsBitset = StrongBitset<std::bitset<6>, MyFlags>;

#ifdef __cpp_lib_constexpr_bitset
	constexpr auto simple_flag = MyFlagsBitset::make_bit(0);
	constexpr auto hard_flag = MyFlagsBitset::make_bit(1);
	constexpr auto basic_flag = MyFlagsBitset::make_bit(2);
	constexpr auto test_flag = MyFlagsBitset::make_bit(3);
#else
	const auto simple_flag = MyFlagsBitset::make_bit(0);
	const auto hard_flag = MyFlagsBitset::make_bit(1);
	const auto basic_flag = MyFlagsBitset::make_bit(2);
	const auto test_flag = MyFlagsBitset::make_bit(3);
#endif

	MyFlagsBitset my_bitset;
	MyFlagsBitset empty_bitset;

	std::bitset<20> test_bitset;
	test_bitset |= 0;
	auto test_bitset2 = test_bitset | std::bitset<20>(0b1);

	std::string s1 = "sdfsdf";
	auto s2 = s1 + std::string("sdfsdf");

	empty_bitset = my_bitset | std::move(empty_bitset);

	std::println("my_bitset: {}", to_string(my_bitset));
	std::println("my_bitset == empty: {}", my_bitset == empty_bitset);

	my_bitset |= simple_flag | hard_flag;

	std::cout << 10;

	std::println("my_bitset: {}", to_string(my_bitset));
	std::println("my_bitset == empty: {}", my_bitset == empty_bitset);
	std::println("my_bitset hash: {}", std::hash<MyFlagsBitset>{}(my_bitset));

	my_bitset &= ~(simple_flag | basic_flag);

	std::println("my_bitset: {}", to_string(my_bitset));

	my_bitset ^= simple_flag | hard_flag | basic_flag;

	std::println("my_bitset: {}", to_string(my_bitset));

	std::println("my_bitset has simple and hard: {}", my_bitset.test(simple_flag | hard_flag));
	std::println("my_bitset has simple and basic: {}", my_bitset.test(simple_flag | basic_flag));

	my_bitset.reset();

	std::println("my_bitset: {}", to_string(my_bitset));
	std::println("my_bitset hash: {}", std::hash<MyFlagsBitset>{}(my_bitset));
	std::println("my_bitset hash: {}", std::hash<MyFlagsBitset>{}(my_bitset));
}

constexpr BitsetLite<20> make_test_bitset() {
	BitsetLite<20> tmp(0b1000);
	tmp.set(2, true);
	tmp[6] = true;
	return tmp;
}

void test_bitset() {
	BitsetLite<20> bitset = 0b1000;
	BitsetLite<20> bitset2 = 0b1010101;

	bitset = bitset | 0b1010101 | 0b1010101 | 0b1010101;
	BitsetLite<20> bitset4 = bitset | bitset2 | 0b111;

	bitset2[1] = true;

	constexpr BitsetLite<20> bitset_const2 = 0b1010101;
	constexpr BitsetLite<20> bitset_const = bitset_const2 | 0b1010101 | 0b1010101 | 0b1010101;

	constexpr BitsetLite<20> bitset_const3 = make_test_bitset();

	BitsetLite<0> test_zero_bitset;
	std::println("sizeof(test_zero_bitset): {}", sizeof(test_zero_bitset));

	[](...) {}(bitset, bitset2);
}

void test_bitset_flags() {
	using MyFlagsBitset = StrongBitset<BitsetLite<128>, struct MyFlagsTag>;

	constexpr auto simple_flag = MyFlagsBitset::make_bit(0);
	constexpr auto hard_flag = MyFlagsBitset::make_bit(1);
	constexpr auto basic_flag = MyFlagsBitset::make_bit(2);
	constexpr auto test_flag = MyFlagsBitset::make_bit(3);
	constexpr auto test_flag2 = MyFlagsBitset::make_bit(100);

	constexpr auto default_flags = simple_flag | basic_flag | test_flag2;

	MyFlagsBitset my_bitset;
	MyFlagsBitset empty_bitset;

	//constexpr auto const_bit = BitsetBit(0);
	//constexpr MyFlagsBitset test_bitset_const(BitsetBit(0));
}

void release_test() {
	//Release release;

	//AsyncClient client;
	//AsyncReleaseGetter release_getter;

	//auto title_getter = release_getter.title();
	//auto title = coro::sync_wait(client.do_request(std::move(title_getter)));

	//std::println("title: {}", title);
}

//template<typename T>
//struct OptionalAwaitable : T {
//
//};

int main() {
	//test_flags();
	//test_bitset();
	//test_bitset_flags();
	release_test();
}