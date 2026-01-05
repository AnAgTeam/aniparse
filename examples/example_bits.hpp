#pragma once
#include <bitset>

template<typename Container, typename Tag>
class StrongBitset {
	static constexpr bool is_notexcept_set = noexcept(std::declval<Container>().set());
	static constexpr bool is_notexcept_reset = noexcept(std::declval<Container>().reset());

public:
	using Bit = BitsetBit;

	//constexpr StrongBitset(const StrongBitset& other) noexcept(std::is_nothrow_copy_constructible_v<Container>) : storage_(other.storage_) {
	//	std::println("Copy ctx");
	//}
	//constexpr StrongBitset(StrongBitset&& other) noexcept(std::is_nothrow_move_constructible_v<Container>) : storage_(std::move(other.storage_)) {
	//	std::println("move ctx");
	//}

	constexpr StrongBitset(const StrongBitset& other) noexcept(std::is_nothrow_copy_constructible_v<Container>) = default;
	constexpr StrongBitset(StrongBitset&& other) noexcept(std::is_nothrow_move_constructible_v<Container>) = default;

	constexpr StrongBitset() noexcept(std::is_nothrow_constructible_v<Container>) = default;
	constexpr explicit StrongBitset(const Container& val) noexcept(std::is_nothrow_copy_constructible_v<Container>) : storage_(val) {}
	constexpr explicit StrongBitset(Container&& val) noexcept(std::is_nothrow_move_constructible_v<Container>) : storage_(std::move(val)) {}

	constexpr explicit StrongBitset(Bit val) noexcept(std::is_nothrow_constructible_v<Container>) {
		storage_.set(static_cast<size_t>(val));
	}

	constexpr explicit operator Container() noexcept {
		return storage_;
	}

	constexpr explicit operator const Container() const noexcept {
		return storage_;
	}

	//constexpr operator bool() noexcept {
	//	return storage_.all();
	//}

	StrongBitset& operator=(const StrongBitset& other) & noexcept = default;
	StrongBitset& operator=(StrongBitset&& other) & noexcept = default;

	[[nodiscard]] decltype(auto) operator[](Bit pos) noexcept(noexcept(std::declval<Container>()[0ULL])) {
		return storage_[static_cast<size_t>(pos)];
	}

	[[nodiscard]] decltype(auto) operator[](Bit pos) const noexcept(noexcept(std::declval<const Container>()[0ULL])) {
		return storage_[static_cast<size_t>(pos)];
	}

	bool all() const noexcept(noexcept(std::declval<Container>().all())) {
		return storage_.all();
	}

	bool any() const noexcept(noexcept(std::declval<Container>().any())) {
		return storage_.any();
	}

	bool none() const noexcept(noexcept(std::declval<Container>().none())) {
		return storage_.none();
	}

	friend bool operator==(const StrongBitset& left, const StrongBitset& right) noexcept {
		return left.storage_ == right.storage_;
	}

	StrongBitset& operator|=(const StrongBitset& other) & noexcept {
		storage_ |= other.storage_;
		return *this;
	}

	StrongBitset& operator&=(const StrongBitset& other) & noexcept {
		storage_ &= other.storage_;
		return *this;
	}

	StrongBitset& operator^=(const StrongBitset& other) & noexcept {
		storage_ ^= other.storage_;
		return *this;
	}

	StrongBitset operator~() const noexcept {
		return StrongBitset{ ~storage_ };
	}

	friend StrongBitset operator|(StrongBitset&& left, const StrongBitset& right) noexcept {
		left |= right;
		return left;
	}

	friend StrongBitset operator|(const StrongBitset& left, StrongBitset&& right) noexcept {
		return std::move(right) | left;
	}

	friend StrongBitset operator|(const StrongBitset& left, const StrongBitset& right) noexcept {
		StrongBitset tmp = left;
		tmp |= right;
		return tmp;
	}

	friend StrongBitset operator&(const StrongBitset& left, const StrongBitset& right) noexcept {
		StrongBitset tmp = left;
		tmp &= right;
		return tmp;
	}

	friend StrongBitset operator&(StrongBitset&& left, const StrongBitset& right) noexcept {
		left &= right;
		return left;
	}

	friend StrongBitset operator&(const StrongBitset& left, StrongBitset&& right) noexcept {
		return std::move(right) & left;
	}

	friend StrongBitset operator^(const StrongBitset& left, const StrongBitset& right) noexcept {
		StrongBitset tmp = left;
		tmp ^= right;
		return tmp;
	}

	friend StrongBitset operator^(StrongBitset&& left, const StrongBitset& right) noexcept {
		left ^= right;
		return left;
	}

	friend StrongBitset operator^(const StrongBitset& left, StrongBitset&& right) noexcept {
		return std::move(right) ^ left;
	}

	bool test(const StrongBitset& values) const noexcept {
		return (*this | values) == values;
	}

	StrongBitset& set() & noexcept(noexcept(std::declval<Container>().set())) {
		storage_.set();
		return *this;
	}

	StrongBitset& reset() & noexcept(noexcept(std::declval<Container>().reset())) {
		storage_.reset();
		return *this;
	}

	/**
	 * Convert bitset to string. '0' for zeros (false) and '1' for ones (true).
	 * @param bitset The bitset to serialize to string
	 * @return String representing the bitset
	 */
	friend std::string to_string(const StrongBitset& bitset) {
		return bitset.storage_.to_string();
	}

	/**
	 * Make a bitset from arbitrary bit at position. Only with constant expressions.
	 * @param pos The position of bit in the bitset
	 * @return Bitset with the bit at 'pos' setted to 1 and others setted to 0
	 */
	static consteval StrongBitset make_bit(size_t pos) noexcept {
		return StrongBitset(BitsetBit(pos));
	}

private:
	Container storage_;
};

using TestBitflags = StrongBitset<std::bitset<20>, struct TestBitflagsTag>;