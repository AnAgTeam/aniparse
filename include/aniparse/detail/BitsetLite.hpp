/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <algorithm>
#include <stdexcept>
#include <limits>

#pragma push_macro("max")
#pragma push_macro("min")
#undef min
#undef max

namespace aniparse::detail {

	/**
	 * @brief Simple bitset just like STL std::bitset, but with constexpr for C++20
	 * @tparam BitCount Size of the bitset in bits
	 */
	template<size_t BitCount>
	class BitsetLite {
		static constexpr size_t char_bits = std::numeric_limits<unsigned char>::digits;

		using Word = std::conditional_t<BitCount <= sizeof(unsigned long) * char_bits, unsigned long, unsigned long long>;

		static constexpr size_t word_bits = char_bits * sizeof(Word);

		/// Minimum count of words needed to store this bits count
		static constexpr size_t word_count = BitCount == 0 ? 0 : (BitCount - 1) / word_bits + 1;

		/// Mask needed for the last word. Used to obtain real value in to_ulong() and to_ullong()
		static constexpr Word last_word_mask = BitCount == word_bits ? ~Word{ 0 } : (Word{ 1 } << (BitCount % word_bits)) - 1;
		
		/// Is it needed to mask the word in to_ulong() and to_ullong()
		static constexpr bool retain_mask_needed = BitCount <= word_bits;

		/**
		 * Class that represents reference to a bit in a bitset.
		 * Can be used to obtain or store bit value
		 */
		struct BitReference {

			/**
			 * @brief Set value in the bitset
			 * @param value New bit value
			 * @return This reference
			 */
			constexpr BitReference& operator=(bool value) noexcept {
				bitset_.set_nothrow(pos_, value);
				return *this;
			}

			/**
			 * @brief Obtain bit current value
			 * @return Bit value
			 */
			constexpr operator bool() const noexcept {
				return bitset_.get_bit(pos_);
			}

			BitsetLite& bitset_;
			size_t pos_;
		};

	public:
		/**
		 * @brief Initialize empty (filled with zeros) bitset
		 */
		constexpr BitsetLite() noexcept : array_() {
			//std::fill(array_, array_ + word_count, Word{ 0 });
		}

		//constexpr BitsetLite(const BitsetLite& other) noexcept {
		//	std::copy(other.array_, other.array_ + word_count + 1, array_);
		//	std::cout << "BitsetLite(const BitsetLite&)\n";
		//}
		//
		//constexpr BitsetLite(BitsetLite&& other) noexcept {
		//	std::copy(other.array_, other.array_ + word_count + 1, array_);
		//	std::cout << "BitsetLite(BitsetLite&&)\n";
		//}
		//
		//constexpr BitsetLite& operator=(const BitsetLite& other) & noexcept {
		//	std::copy(other.array_, other.array_ + word_count + 1, array_);
		//	std::cout << "BitsetLite operator=(const BitsetLite&)\n";
		//	return *this;
		//}
		//constexpr BitsetLite& operator=(BitsetLite&& other) & noexcept {
		//	std::copy(other.array_, other.array_ + word_count + 1, array_);
		//	std::cout << "BitsetLite operator=(BitsetLite&&)\n";
		//	return *this;
		//}

		/**
		 * @brief Initialize bitset with the least bits from the value.
		 * @param value The value with bits
		 */
		constexpr BitsetLite(Word value) noexcept : array_() {
			if constexpr (BitCount == 0) {
				return;
			}
			if constexpr (retain_mask_needed) {
				array_[0] = value & last_word_mask;
			}
			else {
				array_[0] = value;
			}
		}

		/**
		 * Returns maximum bit count, that bitset can hold
		 */
		constexpr size_t size() {
			return BitCount;
		}

		/**
		 * @return true if all the bits are '1', false otherwise
		 */
		constexpr bool all() noexcept {
			return std::find(array_, array_ + word_count, std::numeric_limits<Word>::max()) != array_ + word_count;
		}

		/**
		 * @return true if at least one of the bits are '1', false otherwise
		 */
		constexpr bool any() noexcept {
			return std::find_if(array_, array_ + word_count, std::numeric_limits<Word>::min()) != array_ + word_count;
		}

		/**
		 * @return true if all the bits are '0', false otherwise
		 */
		constexpr bool none() noexcept {
			return !any();
		}

		friend constexpr bool operator==(const BitsetLite& left, const BitsetLite& right) noexcept {
			return std::equal(left.array_, left.array_ + word_count, right.array_);
		}

		friend constexpr bool operator!=(const BitsetLite& left, const BitsetLite& right) noexcept {
			return !(left == right);
		}

		/**
		 * @brief Read the const bit at given position
		 * @param pos The position of the bit starting from 0
		 * @return true if the bit are '1', false otherwise
		 */
		constexpr bool operator[](size_t pos) const noexcept {
			return get_bit(pos);
		}

		/**
		 * @brief Read the bit at given position. The value can be retained or setted after.
		 * @note Setting the bit, that is off limits is undefined behaviour
		 * @param pos The position of the bit starting from 0
		 * @return Reference to the bit in the bitset
		 */
		constexpr BitReference operator[](size_t pos) noexcept {
			return BitReference{ *this, pos };
		}

		/**
		 * @brief Set the bit at given position and check the bounds
		 * @param pos The position of the bit starting from 0
		 * @param value New value of the bit. Defaults to '1' (true)
		 * @throw std::out_of_range If the bit position exceeds bitset size
		 * @return this reference
		 */
		constexpr BitsetLite& set(size_t pos, bool value = true) & {
			if (pos >= BitCount) {
				throw std::out_of_range("Bitset bit subscript out of range");
			}

			set_nothrow(pos, value);
			return *this;
		}

		/**
		 * @brief Set all the bits to '1'
		 * @return this reference
		 */
		constexpr BitsetLite& set() & noexcept {
			std::fill(array_, array_ + word_count, std::numeric_limits<Word>::max());
			return *this;
		}

		/**
		 * @brief Reset the bit (set to '0') at given position and check the bounds
		 * @param pos The position of the bit starting from 0
		 * @throw std::out_of_range If the bit position exceeds bitset size
		 * @return this reference
		 */
		constexpr BitsetLite& reset(size_t pos) & {
			if (pos >= BitCount) {
				throw std::out_of_range("Bitset bit subscript out of range");
			}

			set_nothrow(pos, false);
			return *this;
		}

		/**
		 * @brief Reset all the bits to '0'
		 * @return this reference
		 */
		constexpr BitsetLite& reset() & noexcept {
			std::fill(array_, array_ + word_count, std::numeric_limits<Word>::min());
			return *this;
		}

		/**
		 * @brief Test the bit (check if '1') at given position and check the bounds
		 * @param pos The position of the bit starting from 0
		 * @throw std::out_of_range If the bit position exceeds bitset size
		 * @return true if the bit is '1'
		 */
		constexpr bool test(size_t pos) const {
			if (pos >= BitCount) {
				throw std::out_of_range("Bitset bit subscript out of range");
			}

			return operator[](pos);
		}

		//constexpr BitsetLite& operator|=(BitsetLite& other) noexcept {
		//	std::transform(array_, array_ + word_count, other.array_, array_, [](Word lword, Word rword) {
		//		return lword | rword;
		//	});
		//	return *this;
		//}

		/**
		 * @brief Perform binary OR
		 * @return this reference
		 */
		friend constexpr BitsetLite& operator|=(BitsetLite& left, const BitsetLite& right) noexcept {
			std::transform(left.array_, left.array_ + word_count, right.array_, left.array_, [](Word lword, Word rword) {
				return lword | rword;
			});
			return left;
		}

		/**
		 * @brief Perform binary AND
		 * @return this reference
		 */
		friend constexpr BitsetLite& operator&=(BitsetLite& left, const BitsetLite& right) noexcept {
			std::transform(left.array_, left.array_ + word_count, right.array_, left.array_, [](Word lword, Word rword) {
				return lword & rword;
			});
			return left;
		}

		/**
		 * @brief Perform binary XOR
		 * @return this reference
		 */
		friend constexpr BitsetLite& operator^=(BitsetLite& left, const BitsetLite& right) noexcept {
			std::transform(left.array_, left.array_ + word_count, right.array_, left.array_, [](Word lword, Word rword) {
				return lword ^ rword;
			});
			return left;
		}

		/**
		 * @brief Perform binary NOT
		 * @return New bitset
		 */
		constexpr BitsetLite operator~() noexcept {
			BitsetLite tmp;
			std::transform(array_, array_ + word_count, tmp.array_, [](Word word) {
				return word ^ std::numeric_limits<Word>::max();
			});
			return tmp;
		}

		/**
		 * @brief Perform binary OR
		 * @param left Left operand
		 * @param right Right operand
		 * @return New bitset
		 */
		friend constexpr BitsetLite operator|(const BitsetLite& left, const BitsetLite& right) noexcept {
			BitsetLite tmp = left;
			tmp |= right;
			return tmp;
		}

		/**
		 * @brief Perform binary OR for left operand
		 * @param left R-Value left operand
		 * @param right Right operand
		 * @return R-Value left operand
		 */
		friend constexpr BitsetLite&& operator|(BitsetLite&& left, const BitsetLite& right) noexcept {
			return std::move(left |= right);
		}

		/**
		 * @brief Perform binary OR for right operand
		 * @param left Left operand
		 * @param right R-Value right operand
		 * @return R-Value right operand
		 */
		friend constexpr BitsetLite&& operator|(const BitsetLite& left, BitsetLite&& right) noexcept {
			return std::move(right |= left);
		}

		/**
		 * @brief Perform binary OR for left operand
		 * @param left R-Value left operand
		 * @param right R-Value right operand
		 * @return R-Value left operand
		 */
		friend constexpr BitsetLite&& operator|(BitsetLite&& left, BitsetLite&& right) noexcept {
			return std::move(left |= right);
		}

		/**
		 * @brief Perform binary AND
		 * @param left Left operand
		 * @param right Right operand
		 * @return New bitset
		 */
		friend constexpr BitsetLite operator&(const BitsetLite& left, const BitsetLite& right) noexcept {
			BitsetLite tmp = left;
			tmp &= right;
			return tmp;
		}

		/**
		 * @brief Perform binary AND for left operand
		 * @param left R-Value left operand
		 * @param right Right operand
		 * @return R-Value left operand
		 */
		friend constexpr BitsetLite&& operator&(BitsetLite&& left, const BitsetLite& right) noexcept {
			return std::move(left &= right);
		}

		/**
		 * @brief Perform binary AND for right operand
		 * @param left Left operand
		 * @param right R-Value right operand
		 * @return R-Value right operand
		 */
		friend constexpr BitsetLite&& operator&(const BitsetLite& left, BitsetLite&& right) noexcept {
			return std::move(right &= left);
		}

		/**
		 * @brief Perform binary AND for left operand
		 * @param left R-Value left operand
		 * @param right R-Value right operand
		 * @return R-Value left operand
		 */
		friend constexpr BitsetLite&& operator&(BitsetLite&& left, BitsetLite&& right) noexcept {
			return std::move(left &= right);
		}

		/**
		 * @brief Perform binary XOR
		 * @param left Left operand
		 * @param right Right operand
		 * @return New bitset
		 */
		friend constexpr BitsetLite operator^(const BitsetLite& left, const BitsetLite& right) noexcept {
			BitsetLite tmp = left;
			tmp ^= right;
			return tmp;
		}

		/**
		 * @brief Perform binary XOR for left operand
		 * @param left R-Value left operand
		 * @param right Right operand
		 * @return R-Value left operand
		 */
		friend constexpr BitsetLite&& operator^(BitsetLite&& left, const BitsetLite& right) noexcept {
			return std::move(left ^= right);
		}

		/**
		 * @brief Perform binary AND for right operand
		 * @param left Left operand
		 * @param right R-Value right operand
		 * @return R-Value right operand
		 */
		friend constexpr BitsetLite&& operator^(const BitsetLite& left, BitsetLite&& right) noexcept {
			return std::move(right ^= left);
		}

		/**
		 * @brief Perform binary XOR for left operand
		 * @param left R-Value left operand
		 * @param right R-Value right operand
		 * @return R-Value left operand
		 */
		friend constexpr BitsetLite&& operator^(BitsetLite&& left, BitsetLite&& right) noexcept {
			return std::move(left ^= right);
		}

		/**
		 * @brief Get bitset representation as unsigned long.
		 * Returns only least significant bits
		 * @return Unsigned long representing bitset
		 */
		constexpr unsigned long to_ulong() const noexcept {
			if constexpr (BitCount == 0) {
				return 0UL;
			}
			if constexpr (!retain_mask_needed) {
				return static_cast<unsigned long>(array_[0]);
			}
			else {
				return static_cast<unsigned long>(array_[0] & last_word_mask);
			}
		}

		/**
		 * @brief Get bitset representation as unsigned long long.
		 * Returns only least significant bits
		 * @return Unsigned long long representing bitset
		 */
		constexpr unsigned long long to_ullong() const noexcept {
			if constexpr (BitCount == 0) {
				return 0ULL;
			}
			// We can obtain only lower word, because
			// 1) If BitCount <= bits for unsigned long, then upper word is always zero
			// 2) If BitCount >= bits for unsigned long, then Word is already an unsigned long long
			if constexpr (!retain_mask_needed) {
				return array_[0];
			}
			else {
				return array_[0] & last_word_mask;
			}
		}

	private:

		/**
		 * @brief Read the bit at given position
		 * @param pos The position of the bit starting from 0
		 * @return true if the bit are '1', false otherwise
		 */
		constexpr bool get_bit(size_t pos) const noexcept {
			const Word new_bit = Word{ 1 } << (pos % word_bits);
			return array_[pos / word_bits] & new_bit;
		}

		/**
		 * @brief Set the bit value at given position without checking bounds
		 * @param pos Position of the bit
		 * @param value New value of the bit
		 */
		constexpr void set_nothrow(size_t pos, bool value) noexcept {
			Word& word = array_[pos / (sizeof(Word) * char_bits)];
			const Word new_bit = Word{ 1 } << pos % (sizeof(Word) * char_bits);

			if (value) {
				word |= new_bit;
			}
			else {
				word &= ~new_bit;
			}
		}

		Word array_[word_count == 0 ? 1 : word_count];
	};
}

#pragma pop_macro("min")
#pragma pop_macro("max")