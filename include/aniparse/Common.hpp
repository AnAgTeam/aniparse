#pragma once
#include "aniparse/FlagsBitfield.hpp"
#include "aniparse/ClientContext.hpp"
#include <string>
#include <vector>

namespace aniparse {
	using TagID = int;
	using ImageID = int;
	using SeriesID = int;
	using UserListID = int;

	using CompatibilitiesFlags = FlagsBitfield<64, struct CompatibilitiesFlagsTag>;

	namespace compatibilities_flags {
		constexpr auto supports_inplace_get = CompatibilitiesFlags::make_bit(0);
		constexpr auto supports_images_store = CompatibilitiesFlags::make_bit(1);
		constexpr auto supports_anime_store = CompatibilitiesFlags::make_bit(2);
		constexpr auto supports_manga_store = CompatibilitiesFlags::make_bit(3);
		constexpr auto supports_video_store = CompatibilitiesFlags::make_bit(4);
		constexpr auto using_custom_store = CompatibilitiesFlags::make_bit(5);
		constexpr auto adult_source = CompatibilitiesFlags::make_bit(6);

		constexpr auto supports_images_search = CompatibilitiesFlags::make_bit(7);
		constexpr auto supports_registration = CompatibilitiesFlags::make_bit(8);
		constexpr auto supports_voting = CompatibilitiesFlags::make_bit(9);
		constexpr auto supports_commenting = CompatibilitiesFlags::make_bit(10);
		constexpr auto supports_online_lists = CompatibilitiesFlags::make_bit(11);

		constexpr CompatibilitiesFlags default_flags;
	}

	/// Minimal age allowed to access the item. Use 0 for all
	using AgeRestriction = int;

	struct Image {
		TagID id;
		std::string url;
		int width;
		int height;
	};

	struct Tag {
		TagID id;
		std::string name;
		std::string referer;
	};

	struct RelatedUser {
		std::string name;
		std::string referer;
	};

	struct Series {
		SeriesID id;
		std::string name;
		std::string referer;
	};

	/**
	 * @see AiredStatus::make_default()
	 * Default supported AiredStatuses.
	 * Used to create generic AiredStatus.
	 */
	enum class DefaultAiredStatuses {
		Released,
		Ongoing,
		Announced
	};

	struct AiredStatus {
		std::string name;
		std::chrono::system_clock::time_point time;

		static AiredStatus make_default(AiredStatus status, std::chrono::system_clock::time_point time);
	};

	/**
	 * Structure representing rating (score) of the item (release, manga, etc.)
	 */
	struct Rating {
		/// Release rating is from 0 to 10. If the rating is from 0 to 5, just multiple it by 2 (3/5 -> 6/10)
		int total_rating;
		/// Total sum of the 'rating' field. Used to calculate overall rating
		int total_raters;
		/// Maximum rate in the 'rating' field, cannot be more than 10. Often sets to 2/5/10
		int max_rating;
		/// Count for each of the rates positions;
		std::array<int, 10> rating;

		//static Rating make_simple(int overall_rating);
	};

	/**
	 * @see UserList::make_default()
	 * Default supported UserLists.
	 * Used to create generic UserList.
	 */
	enum class DefaultUserLists {
		Planning,
		Dropped,
		Favorite,
		Other,
		// video related
		Watched,
		Watching,
		// book related
		Reading,
		Read,
	};

	/**
	 * Structure representing list, that users added
	 * the item. Usually in release, manga headers
	 */
	struct UserList {
		UserListID id;
		std::string name;

		static UserList make_default(DefaultUserLists user_list, UserListID id = UserListID{});
	};

	struct ViewStats {
		int views = 0;
	};

	template<typename T, typename Container = std::vector<T>>
	struct ForwardPaginator {
		virtual ~ForwardPaginator() = default;

		virtual asyncnet::NetworkTask<Container> next(ClientContext& client) = 0;

		virtual bool end() const = 0;

		virtual asyncnet::NetworkTask<size_t> total_items() = 0;
	};

	struct GetFilters {

	};
}