/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <boost/regex.hpp>
#include <ranges>
#include <string_view>

namespace aniparse {
	using boost::basic_regex;
	using boost::regex;
	using boost::wregex;

	namespace regex_constants {
		using namespace boost::regex_constants;
	}

	using boost::sub_match;
	using boost::csub_match;
	using boost::ssub_match;
	using boost::wssub_match;

	using boost::match_results;
	using boost::cmatch;
	using boost::wcmatch;
	using boost::smatch;
	using boost::wsmatch;
	using svmatch = match_results<std::string_view::const_iterator>;
	using wsvmatch = match_results<std::wstring_view::const_iterator>;

	using boost::regex_match;
	using boost::regex_search;
	using boost::regex_replace;

	using boost::regex_token_iterator;
	using boost::cregex_token_iterator;
	using boost::sregex_token_iterator;
	using boost::wsregex_token_iterator;

	template<
		std::ranges::range Range,
		typename Alloc,
		typename TChar,
		typename Traits>
		requires std::convertible_to<std::ranges::range_value_t<Range>, TChar>
			&& std::bidirectional_iterator<std::ranges::iterator_t<Range>>
	inline bool regex_search(
		Range&& range,
		match_results<std::ranges::iterator_t<Range>, Alloc>& results,
		basic_regex<TChar, Traits>& reg,
		regex_constants::match_flag_type flags = regex_constants::match_default) {
		return regex_search(
			std::begin(range), std::end(range),
			results, reg, flags);
	}

	template<
		std::ranges::range Range,
		typename Alloc,
		typename TChar,
		typename Traits>
		requires std::convertible_to<std::ranges::range_value_t<Range>, TChar>
			&& std::bidirectional_iterator<std::ranges::iterator_t<Range>>
	inline bool regex_match(
		Range&& range,
		match_results<std::ranges::iterator_t<Range>, Alloc>& results,
		basic_regex<TChar, Traits>& reg,
		regex_constants::match_flag_type flags = regex_constants::match_default) {
		return regex_match(
			std::begin(range), std::end(range),
			results, reg, flags);
	}
}