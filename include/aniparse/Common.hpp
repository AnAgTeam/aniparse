/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
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

	using FilteringFlags = FlagsBitfield<64, struct FilteringFlagsTag>;

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

		/**
		 * All the Paginator<T>::next items will be unique over time if true.
		 * Otherwise, the items can duplicate. For example, when you get 10
		 * from next, then 3 new items added at the start and you start getting
		 * duplicates of 8-10 from previous parsing and only 7 new
		 */
		constexpr auto supports_pagination_uniqueness = CompatibilitiesFlags::make_bit(12);

		constexpr auto unsupported_feature = CompatibilitiesFlags::make_bit(13);

		constexpr CompatibilitiesFlags default_flags;
	}

	namespace filtering_flags {
		constexpr auto sort_popularity_desc = FilteringFlags::make_bit(0);
		constexpr auto sort_popularity_asc = FilteringFlags::make_bit(1);
		constexpr auto sort_rating_desc = FilteringFlags::make_bit(2);
		constexpr auto sort_rating_asc = FilteringFlags::make_bit(3);
		constexpr auto sort_views_desc = FilteringFlags::make_bit(4);
		constexpr auto sort_views_asc = FilteringFlags::make_bit(5);
		constexpr auto sort_title_desc = FilteringFlags::make_bit(6);
		constexpr auto sort_title_asc = FilteringFlags::make_bit(7);
		constexpr auto sort_release_time_desc = FilteringFlags::make_bit(8);
		constexpr auto sort_release_time_asc = FilteringFlags::make_bit(9);
		constexpr auto sort_update_time_desc = FilteringFlags::make_bit(10);
		constexpr auto sort_update_time_asc = FilteringFlags::make_bit(11);
		constexpr auto sort_downloads_desc = FilteringFlags::make_bit(12);
		constexpr auto sort_downloads_asc = FilteringFlags::make_bit(13);
		constexpr auto sort_author_desc = FilteringFlags::make_bit(14);
		constexpr auto sort_author_asc = FilteringFlags::make_bit(15);

		constexpr auto sort_popularity = sort_popularity_desc | sort_popularity_asc;
		constexpr auto sort_rating = sort_rating_desc | sort_rating_asc;
		constexpr auto sort_views = sort_views_desc | sort_views_asc;
		constexpr auto sort_title = sort_title_desc | sort_title_asc;
		constexpr auto sort_release_time = sort_release_time_desc | sort_release_time_asc;
		constexpr auto sort_update_time = sort_update_time_desc | sort_update_time_asc;
		constexpr auto sort_downloads = sort_downloads_desc | sort_downloads_asc;
		constexpr auto sort_author = sort_author_desc | sort_author_asc;

		constexpr FilteringFlags default_flags;
	}

	enum class FilterSort {
		None,
		PopularityDesc,
		PopularityAsc,
		RatingDesc,
		RatingAsc,
		ViewsDesc,
		ViewsAsc,
		TitleDesc,
		TitleAsc,
		ReleaseTimeDesc,
		ReleaseTimeAsc,
		UpdateTimeDesc,
		UpdateTimeAsc,
		DownloadsDesc,
		DownloadsAsc,
		AuthorDesc,
		AuthorAsc
	};

	namespace text_attributes {
		struct TextLink {
			std::string url;
		};
	}

	/// Minimal age allowed to access the item. Use 0 for all
	using AgeRestriction = int;

	struct ImageResolution {
		int width;
		int height;
	};

	struct Image {
		TagID id{ 0 };
		std::string url;
		std::optional<ImageResolution> size;
	};

	struct Tag {
		TagID id{ 0 };
		std::string name;
		std::string referer;
	};

	struct RelatedUser {
		std::string name;
		std::string referer;
	};

	struct Series {
		SeriesID id{ 0 };
		std::string name;
		std::string referer;

		static Series make_original(std::string referer);
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

		static AiredStatus make_default(DefaultAiredStatuses status, std::chrono::system_clock::time_point time);
	};

	/**
	 * Structure representing rating (score) of the item (release, manga, etc.)
	 */
	struct Rating {
		/// Release rating is from 0 to 10. If the rating is from 0 to 5, just multiple it by 2 (3/5 -> 6/10)
		int total_rating = 0;
		/// Total sum of the 'rating' field. Used to calculate overall rating
		int total_raters = 0;
		/// Maximum rate in the 'rating' field, cannot be more than 10. Often sets to 2/5/10
		int max_rating = 0;
		/// Count for each of the rates positions;
		std::array<int, 10> rating = {};

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

	struct TextAttributeInfo {
		int start;
		int end;
		std::variant<text_attributes::TextLink> data;
	};

	/**
	 * Structure representing text with some attributes
	 * in it like links, colors, etc.
	 */
	struct AttributedText {
		std::string text;
		std::vector<TextAttributeInfo> attributes;
	};

	template<typename T, typename TContainer = std::vector<T>>
	struct ForwardPaginator {
		using Container = TContainer;

		virtual ~ForwardPaginator() = default;

		virtual asyncnet::NetworkTask<Container> current(RequestorContext context) = 0;

		virtual asyncnet::NetworkTask<Container> next(RequestorContext context) = 0;

		virtual bool end() const = 0;

		virtual asyncnet::NetworkTask<size_t> total_items(RequestorContext context) = 0;
	};

	using pageoff = std::ptrdiff_t;
	constexpr size_t page_no_limit = static_cast<size_t>(-1);

	constexpr std::chrono::system_clock::time_point unknown_time{ std::chrono::system_clock::duration{0} };

	template<typename T>
	struct PageItem {
		T item;
		pageoff offset;
	};

	template<typename T, typename Container = std::vector<PageItem<T>>>
	struct PageResults {
		Container results;
		pageoff next_offset = pageoff(0);
		size_t total_count = 0;
	};

	template<typename T, typename Container = PageResults<T>>
	struct ForwardHashedPaginator {
		//using PageResults = PageItemsResults<T, TContainer>;
		using PageResults = Container;

		static constexpr size_t no_limit = static_cast<size_t>(-1);

		virtual ~ForwardHashedPaginator() = default;

		virtual asyncnet::NetworkTask<PageResults> get_from(
			RequestorContext context,
			pageoff pos,
			size_t limit = no_limit) = 0;
	};

	struct GetFilters {
		pageoff from = 0;
		size_t limit = page_no_limit;
		FilterSort sort = FilterSort::None;
	};
}