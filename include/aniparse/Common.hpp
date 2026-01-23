/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"
#include "aniparse/ClientContext.hpp"
#include <string>
#include <vector>
#include <variant>
#include <map>

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

		constexpr auto sort_popularity_desc_hour = FilteringFlags::make_bit(16);
		constexpr auto sort_popularity_desc_week = FilteringFlags::make_bit(17);
		constexpr auto sort_popularity_desc_month = FilteringFlags::make_bit(18);
		constexpr auto sort_popularity_desc_year = FilteringFlags::make_bit(19);

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
		AuthorAsc,

		PopulariryDescHour,
		PopulariryDescWeek,
		PopulariryDescMonth,
		PopulariryDescYear,
	};

	struct Hyperlink {
		std::string url;
	};

	/// Minimal age allowed to access the item. Use 0 for all
	using AgeRestriction = int;

	struct ImageResolution {
		int width;
		int height;
	};

	struct Image {
		ImageID id{ 0 };
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

	constexpr std::string_view series_original = "original";

	struct Series {
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
		Announced,
		Other,
	};

	constexpr std::string_view aired_status_released  = "released";
	constexpr std::string_view aired_status_ongoing	  = "ongoing";
	constexpr std::string_view aired_status_announced = "announced";

	struct AiredStatus {
		std::string name;
		std::chrono::system_clock::time_point time;

		DefaultAiredStatuses to_enum() const;
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

	constexpr std::string_view user_list_planning	= "planning";
	constexpr std::string_view user_list_dropped	= "dropped";
	constexpr std::string_view user_list_favorite	= "favorite";
	constexpr std::string_view user_list_watched	= "watched";
	constexpr std::string_view user_list_watching	= "watching";
	constexpr std::string_view user_list_reading	= "reading";
	constexpr std::string_view user_list_read		= "read";

	/**
	 * Structure representing list, that users added
	 * the item. Usually in release, manga headers
	 */
	struct UserList {
		std::string name;

		DefaultUserLists to_enum() const;
	};

	struct ViewStats {
		int views = 0;
	};

	struct TextAttributeInfo {
		int start;
		int end;
		std::variant<Hyperlink> data;
	};

	/**
	 * Structure representing text with some attributes
	 * in it like links, colors, etc.
	 */
	struct AttributedText {
		std::string text;
		std::vector<TextAttributeInfo> attributes;
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

	struct GetFilters {
		pageoff from = 0;
		size_t limit = page_no_limit;
		FilterSort sort = FilterSort::None;
	};


	// search

	/**
	 * @brief One text input
	 */
	struct TextQuery {
		/**
		 * Search context: text value of an item
		 * Support check: Name of input
		 */
		std::string text;
		/**
		 * Search context: Is text inversed (i.e. include not containing text)
		 * Support check: Is inverse supported
		 */
		bool exclusive = false;
	};

	enum class DirectionalIntervalOperator {
		None,
		More,
		MoreExact,
		Less,
		LessExact,
		Exact,
	};

	/**
	 * @brief Integer interval input
	 * For example pages count: (dir = MoreExact, count = 10) -> pages >= 10
	 */
	struct DirectionalInterval {
		/**
		 * Search context: Value of an item
		 * Support check: Min/max value of item
		 */
		size_t count = 0;
		DirectionalIntervalOperator direction = DirectionalIntervalOperator::None;
	};

	/**
	 * @brief Integer two-side interval input
	 * For example pages count: (from = 5, to = 10) -> 5 <= pages <= 10
	 * (from = 5, to = 10, exclusive) -> 5 >= pages >= 10
	 * @note "from" shouldn't be less that "to", but must be expected as error
	 */
	struct BidirectionalInterval {
		/**
		 * Search context: Start of iterval
		 * Support check: Min value of interval
		 */
		std::ptrdiff_t from = 0;
		/**
		 * Search context: End of interval
		 * Support check: Max value of interval
		 */
		std::ptrdiff_t to = 0;
		/**
		 * Search context: Is interval exclusive (not containing in interval)
		 * Support check: Is exclusive supported
		 */
		bool exclusive = false;
	};

	enum class TimeIntervalPrecision {
		Any,
		Now,
		Hour,
		Day,
		Week,
		Month,
		Year,
	};

	/**
	 * @brief Time interval input
	 * Same as @see DirectionalInterval
	 */
	struct TimeInterval {
		std::chrono::system_clock::time_point from = unknown_time;
		DirectionalIntervalOperator direction = DirectionalIntervalOperator::None;
	};

	/**
	 * @brief Time two-side interval input
	 * Same as @see BidirectionalInterval
	 */
	struct BidirectionalTimeInterval {
		std::chrono::system_clock::time_point from = unknown_time;
		std::chrono::system_clock::time_point to = unknown_time;
		bool exclusive = false;
	};

	//struct TypedQueryItem {
	//	std::string type;
	//	std::string name;
	//};

	struct ItemSelectionValue {
		std::string value;
		bool enabled;
		bool exclusive;
	};

	using ItemSelection = std::map<std::string, ItemSelectionValue, std::less<>>;

	/**
	 * @brief Switch/checkmark. If presented means true
	 *        exclusion (if supported).
	 */
	struct Checkmark {
		bool exclusive = false;
	};

	using SearchItemVariant = std::variant<
		TextQuery,
		DirectionalInterval,
		BidirectionalInterval,
		//TypedQueryItem,
		ItemSelection,
		Checkmark>;

	namespace search_keys {
		/// Filter by @see Series. Usually TextQuery
		constexpr std::string_view series = "series";
		/// Filter by pages/episodes count. Usually DirectionalInterval/BidirectionalInterval
		constexpr std::string_view pages = "icount";
		constexpr std::string_view episodes = "icount";
		/// Filter by Tag. Usually TextQuery
		constexpr std::string_view tag = "tag";
		/// Filter by upload time (last time when item was updated on specific page). Usually DirectionalInterval/BidirectionalInterval
		constexpr std::string_view upload_time = "upd_time";
		/// Filter by release time (actual time when item was released). Usually DirectionalInterval/BidirectionalInterval
		constexpr std::string_view release_time = "rel_time";
		/// Filter by title, the text must be contained in title, but maybe not fully. Usually TextQuery
		constexpr std::string_view title = "title";
		/// Filter by status like "Announced", "Released" (Aired state of item). Usually ? (TextQuery)
		constexpr std::string_view status = "status";
		/// Filter by Rating. ? (DirectionalInterval/BidirectionalInterval)
		constexpr std::string_view rating = "rating";
		/// Filter by year. ? (DirectionalInterval/BidirectionalInterval)
		constexpr std::string_view year = "year";
		/// Filter by AgeRestriction. Usually DirectionalInterval/ItemSelection
		constexpr std::string_view age_restriction = "age_res";
	}

	struct SearchRequestQuery {
		std::string query;
		std::map<std::string, SearchItemVariant, std::less<>> filters;

		//template<typename Value>
		//SearchItemVariant& insert_filter(Value&& value) {

		//}
	};

	enum class SearchItemClass {
		None,
		TextQuery,
		Interval,
		BidirectInterval,
		TypedQuery,
		Checkmark,
	};

	struct SerializedGetterData {
		std::string url;
		std::map<std::string, std::string, std::less<>> params;
	};
}