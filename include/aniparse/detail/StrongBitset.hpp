/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <string>

namespace aniparse::detail {

class BitsetBit {
public:
	explicit constexpr BitsetBit(size_t pos) noexcept : pos_(pos) {}

	explicit constexpr operator size_t() const noexcept {
		return pos_;
	}

private:
	size_t pos_;
};

template <typename Container, typename Tag>
class StrongBitset {
	friend std::hash<StrongBitset>;

public:
	using Bit = BitsetBit;

	constexpr StrongBitset(const StrongBitset& other) noexcept(std::is_nothrow_copy_constructible_v<Container>) = default;
	constexpr StrongBitset(StrongBitset&& other) noexcept(std::is_nothrow_move_constructible_v<Container>)      = default;

	constexpr StrongBitset() noexcept(std::is_nothrow_constructible_v<Container>) = default;
	constexpr explicit StrongBitset(const Container& val) noexcept(std::is_nothrow_copy_constructible_v<Container>) : storage_(val) {}
	constexpr explicit StrongBitset(Container&& val) noexcept(std::is_nothrow_move_constructible_v<Container>) : storage_(std::move(val)) {}

	constexpr explicit StrongBitset(Bit val) noexcept(
	    std::is_nothrow_default_constructible_v<Container> &&
	    noexcept(std::declval<Container&>().set(size_t{}))) {
		storage_.set(static_cast<size_t>(val));
	}

	constexpr explicit operator Container() noexcept(std::is_nothrow_copy_constructible_v<Container>) {
		return storage_;
	}

	constexpr explicit operator const Container() const noexcept(std::is_nothrow_copy_constructible_v<Container>) {
		return storage_;
	}

	constexpr StrongBitset& operator=(const StrongBitset& other) & noexcept(std::is_nothrow_copy_assignable_v<Container>) = default;
	constexpr StrongBitset& operator=(StrongBitset&& other) & noexcept(std::is_nothrow_move_assignable_v<Container>) = default;

	[[nodiscard]] constexpr decltype(auto) operator[](Bit pos) noexcept(noexcept(std::declval<Container&>()[size_t{}])) {
		return storage_[static_cast<size_t>(pos)];
	}

	[[nodiscard]] constexpr decltype(auto) operator[](Bit pos) const noexcept(noexcept(std::declval<const Container&>()[size_t{}])) {
		return storage_[static_cast<size_t>(pos)];
	}

	constexpr bool all() const noexcept(noexcept(std::declval<const Container&>().all())) {
		return storage_.all();
	}

	constexpr bool any() const noexcept(noexcept(std::declval<const Container&>().any())) {
		return storage_.any();
	}

	constexpr bool none() const noexcept(noexcept(std::declval<const Container&>().none())) {
		return storage_.none();
	}

	constexpr friend bool operator==(const StrongBitset& left, const StrongBitset& right)
	    noexcept(noexcept(left.storage_ == right.storage_)) {
		return left.storage_ == right.storage_;
	}

	constexpr StrongBitset& operator|=(const StrongBitset& other) &
	    noexcept(noexcept(std::declval<Container&>() |= std::declval<const Container&>())) {
		storage_ |= other.storage_;
		return *this;
	}

	constexpr StrongBitset& operator&=(const StrongBitset& other) &
	    noexcept(noexcept(std::declval<Container&>() &= std::declval<const Container&>())) {
		storage_ &= other.storage_;
		return *this;
	}

	constexpr StrongBitset& operator^=(const StrongBitset& other) &
	    noexcept(noexcept(std::declval<Container&>() ^= std::declval<const Container&>())) {
		storage_ ^= other.storage_;
		return *this;
	}

	constexpr StrongBitset operator~() const noexcept(noexcept(StrongBitset{~std::declval<const Container&>()})) {
		return StrongBitset{~storage_};
	}

	constexpr friend StrongBitset operator|(StrongBitset&& left, const StrongBitset& right)
	    noexcept(noexcept(left |= right) && std::is_nothrow_move_constructible_v<Container>) {
		left |= right;
		return std::move(left);
	}

	constexpr friend StrongBitset operator|(StrongBitset&& left, StrongBitset&& right)
	    noexcept(noexcept(std::move(left) | static_cast<const StrongBitset&>(right))) {
		return std::move(left) | static_cast<const StrongBitset&>(right);
	}

	constexpr friend StrongBitset operator|(const StrongBitset& left, StrongBitset&& right)
	    noexcept(noexcept(std::move(right) | left)) {
		return std::move(right) | left;
	}

	constexpr friend StrongBitset operator|(const StrongBitset& left, const StrongBitset& right)
	    noexcept(std::is_nothrow_copy_constructible_v<Container> && noexcept(std::declval<StrongBitset&>() |= right) &&
	             std::is_nothrow_move_constructible_v<Container>) {
		StrongBitset tmp = left;
		tmp |= right;
		return tmp;
	}

	constexpr friend StrongBitset operator&(StrongBitset&& left, const StrongBitset& right)
	    noexcept(noexcept(left &= right) && std::is_nothrow_move_constructible_v<Container>) {
		left &= right;
		return std::move(left);
	}

	constexpr friend StrongBitset operator&(StrongBitset&& left, StrongBitset&& right)
	    noexcept(noexcept(std::move(left) & static_cast<const StrongBitset&>(right))) {
		return std::move(left) & static_cast<const StrongBitset&>(right);
	}

	constexpr friend StrongBitset operator&(const StrongBitset& left, StrongBitset&& right)
	    noexcept(noexcept(std::move(right) & left)) {
		return std::move(right) & left;
	}

	constexpr friend StrongBitset operator&(const StrongBitset& left, const StrongBitset& right)
	    noexcept(std::is_nothrow_copy_constructible_v<Container> && noexcept(std::declval<StrongBitset&>() &= right) &&
	             std::is_nothrow_move_constructible_v<Container>) {
		StrongBitset tmp = left;
		tmp &= right;
		return tmp;
	}

	constexpr friend StrongBitset operator^(StrongBitset&& left, const StrongBitset& right)
	    noexcept(noexcept(left ^= right) && std::is_nothrow_move_constructible_v<Container>) {
		left ^= right;
		return std::move(left);
	}

	constexpr friend StrongBitset operator^(StrongBitset&& left, StrongBitset&& right)
	    noexcept(noexcept(std::move(left) ^ static_cast<const StrongBitset&>(right))) {
		return std::move(left) ^ static_cast<const StrongBitset&>(right);
	}

	constexpr friend StrongBitset operator^(const StrongBitset& left, StrongBitset&& right)
	    noexcept(noexcept(std::move(right) ^ left)) {
		return std::move(right) ^ left;
	}

	constexpr friend StrongBitset operator^(const StrongBitset& left, const StrongBitset& right)
	    noexcept(std::is_nothrow_copy_constructible_v<Container> && noexcept(std::declval<StrongBitset&>() ^= right) &&
	             std::is_nothrow_move_constructible_v<Container>) {
		StrongBitset tmp = left;
		tmp ^= right;
		return tmp;
	}

	/**
	 * @brief Whether every bit of @p values is set in this bitset — i.e. this
	 * has @p values. For a single-bit flag, "is the flag set"; for a multi-bit
	 * mask, "are all of these flags set". Named has() rather than test() to avoid
	 * the std::bitset::test(pos) reading: this takes a mask, not a bit position.
	 * @param values The flag bit(s) to look for.
	 * @return true iff (*this & values) == values.
	 */
	constexpr bool has(const StrongBitset& values) const noexcept(noexcept((*this & values) == values)) {
		return (*this & values) == values;
	}

	constexpr StrongBitset& set() & noexcept(noexcept(std::declval<Container&>().set())) {
		storage_.set();
		return *this;
	}

	constexpr StrongBitset& reset() & noexcept(noexcept(std::declval<Container&>().reset())) {
		storage_.reset();
		return *this;
	}

	/**
	 * @brief Convert bitset to string. '0' for zeros (false) and '1' for ones (true).
	 * @param bitset The bitset to serialize to string
	 * @return String representing the bitset
	 */
	friend std::string to_string(const StrongBitset& bitset) noexcept(noexcept(std::string(bitset.storage_.to_string()))) {
		return bitset.storage_.to_string();
	}

	/**
	 * @brief Make a bitset from arbitrary bit at position.
	 * @param pos The position of bit in the bitset
	 * @return Bitset with the bit at pos set to 1 and all other bits set to 0.
	 */
	static constexpr StrongBitset make_bit(size_t pos) noexcept(noexcept(StrongBitset(Bit(pos)))) {
		return StrongBitset(Bit(pos));
	}

private:
	Container storage_;
};

} // namespace aniparse::detail

template <typename Container, typename Tag>
    requires requires(Container a) { std::hash<Container>{}(a); }
struct std::hash<aniparse::detail::StrongBitset<Container, Tag>> {
	size_t operator()(const aniparse::detail::StrongBitset<Container, Tag>& other) const noexcept(noexcept(std::hash<Container>{}(other.storage_))) {
		return std::hash<Container>{}(other.storage_);
	}
};
