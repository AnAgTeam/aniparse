#pragma once
#include "aniparse/FlagsBitfield.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <memory>

namespace aniparse {
	class AsyncReleaseGetter;

	using ParseFlags = FlagsBitfield<64, struct ParseFlagsTag>;
	using CompatibilitiesFlags = FlagsBitfield<64, struct CompatibilitiesFlagsTag>;

	namespace parse_flags {
		constexpr auto supports_inplace_get = ParseFlags::make_bit(0);

		constexpr ParseFlags default_flags;
	}

	namespace compatibilities_flags {
		constexpr auto supports_inplace_get = CompatibilitiesFlags::make_bit(0);
		constexpr auto supports_images_store = CompatibilitiesFlags::make_bit(1);
		constexpr auto supports_anime_store = CompatibilitiesFlags::make_bit(2);
		constexpr auto supports_hanime_store = CompatibilitiesFlags::make_bit(3);
		constexpr auto using_custom_store = CompatibilitiesFlags::make_bit(4);

		constexpr CompatibilitiesFlags default_flags;
	}

	struct ParseContext {
		std::string url;
	};

	struct ParseQueryResult {

		ParseFlags flags = parse_flags::default_flags;
	};

	struct ParserCompatibilities {

		CompatibilitiesFlags flags = compatibilities_flags::default_flags;
	};

	template<typename T>
	struct ParseResult {
		T data;
		ParseQueryResult meta;
	};

	struct Parser {
		virtual ~Parser() = default;

		virtual ParserCompatibilities compatibilities() const = 0;

		//virtual ParseResult<std::unique_ptr<AsyncReleaseGetter>> async_release_getter(ExtractionContext& ctx);


	};

	//using ParserProvider = std::function<
}