#pragma once
#include "aniparse/FlagsBitfield.hpp"
#include "aniparse/images/Image.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <memory>
#include <map>

namespace aniparse {
	class AsyncReleaseGetter;
	class ImagesGetter;

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
		constexpr auto supports_manga_store = CompatibilitiesFlags::make_bit(3);
		constexpr auto using_custom_store = CompatibilitiesFlags::make_bit(4);
		constexpr auto supports_images_search = CompatibilitiesFlags::make_bit(5);

		constexpr CompatibilitiesFlags default_flags;
	}

	struct ParseContext {
		std::string url;

		std::map<std::string, std::string> user_args;
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

		virtual std::string identifier() const = 0;

		virtual bool valid_for_url(std::string_view url) const = 0;

		virtual ParserCompatibilities compatibilities() const = 0;

		//virtual ParseResult<std::unique_ptr<AsyncReleaseGetter>> async_release_getter(ParseContext& ctx);

		virtual std::unique_ptr<ImagesGetter> images_getter() const;
	};

	//using ParserProvider = std::function<
}