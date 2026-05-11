/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Expected.hpp"
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
		inline constexpr auto supports_inplace_get   = CompatibilitiesFlags::make_bit(0);
		inline constexpr auto supports_images_store  = CompatibilitiesFlags::make_bit(1);
		inline constexpr auto supports_anime_store   = CompatibilitiesFlags::make_bit(2);
		inline constexpr auto supports_manga_store   = CompatibilitiesFlags::make_bit(3);
		inline constexpr auto supports_video_store   = CompatibilitiesFlags::make_bit(4);
		inline constexpr auto using_custom_store     = CompatibilitiesFlags::make_bit(5);
		inline constexpr auto adult_source           = CompatibilitiesFlags::make_bit(6);

		inline constexpr auto supports_images_search = CompatibilitiesFlags::make_bit(7);
		inline constexpr auto supports_registration  = CompatibilitiesFlags::make_bit(8);
		inline constexpr auto supports_voting        = CompatibilitiesFlags::make_bit(9);
		inline constexpr auto supports_commenting    = CompatibilitiesFlags::make_bit(10);
		inline constexpr auto supports_online_lists  = CompatibilitiesFlags::make_bit(11);

		/**
		 * All the Paginator<T>::next items will be unique over time if true.
		 * Otherwise, the items can duplicate. For example, when you get 10
		 * from next, then 3 new items added at the start and you start getting
		 * duplicates of 8-10 from previous parsing and only 7 new
		 */
		inline constexpr auto supports_pagination_uniqueness = CompatibilitiesFlags::make_bit(12);

		inline constexpr auto unsupported_feature    = CompatibilitiesFlags::make_bit(13);

		inline constexpr CompatibilitiesFlags default_flags;
	}

	namespace filtering_flags {
		inline constexpr auto sort_popularity_desc		= FilteringFlags::make_bit(0);
		inline constexpr auto sort_popularity_asc		= FilteringFlags::make_bit(1);
		inline constexpr auto sort_rating_desc			= FilteringFlags::make_bit(2);
		inline constexpr auto sort_rating_asc			= FilteringFlags::make_bit(3);
		inline constexpr auto sort_views_desc			= FilteringFlags::make_bit(4);
		inline constexpr auto sort_views_asc			= FilteringFlags::make_bit(5);
		inline constexpr auto sort_title_desc			= FilteringFlags::make_bit(6);
		inline constexpr auto sort_title_asc			= FilteringFlags::make_bit(7);
		inline constexpr auto sort_release_time_desc	= FilteringFlags::make_bit(8);
		inline constexpr auto sort_release_time_asc		= FilteringFlags::make_bit(9);
		inline constexpr auto sort_update_time_desc		= FilteringFlags::make_bit(10);
		inline constexpr auto sort_update_time_asc		= FilteringFlags::make_bit(11);
		inline constexpr auto sort_downloads_desc		= FilteringFlags::make_bit(12);
		inline constexpr auto sort_downloads_asc		= FilteringFlags::make_bit(13);
		inline constexpr auto sort_author_desc			= FilteringFlags::make_bit(14);
		inline constexpr auto sort_author_asc			= FilteringFlags::make_bit(15);

		inline constexpr auto sort_popularity_desc_hour  = FilteringFlags::make_bit(16);
		inline constexpr auto sort_popularity_desc_day   = FilteringFlags::make_bit(17);
		inline constexpr auto sort_popularity_desc_week  = FilteringFlags::make_bit(18);
		inline constexpr auto sort_popularity_desc_month = FilteringFlags::make_bit(19);
		inline constexpr auto sort_popularity_desc_year  = FilteringFlags::make_bit(20);

		inline constexpr auto sort_popularity   = sort_popularity_desc | sort_popularity_asc;
		inline constexpr auto sort_rating       = sort_rating_desc | sort_rating_asc;
		inline constexpr auto sort_views        = sort_views_desc | sort_views_asc;
		inline constexpr auto sort_title        = sort_title_desc | sort_title_asc;
		inline constexpr auto sort_release_time = sort_release_time_desc | sort_release_time_asc;
		inline constexpr auto sort_update_time  = sort_update_time_desc | sort_update_time_asc;
		inline constexpr auto sort_downloads    = sort_downloads_desc | sort_downloads_asc;
		inline constexpr auto sort_author       = sort_author_desc | sort_author_asc;

		inline constexpr FilteringFlags default_flags;
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

	inline constexpr std::string_view aired_status_released  = "released";
	inline constexpr std::string_view aired_status_ongoing   = "ongoing";
	inline constexpr std::string_view aired_status_announced = "announced";

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

	inline constexpr std::string_view user_list_planning = "planning";
	inline constexpr std::string_view user_list_dropped  = "dropped";
	inline constexpr std::string_view user_list_favorite = "favorite";
	inline constexpr std::string_view user_list_watched	 = "watched";
	inline constexpr std::string_view user_list_watching = "watching";
	inline constexpr std::string_view user_list_reading  = "reading";
	inline constexpr std::string_view user_list_read     = "read";

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
	inline constexpr size_t page_no_limit = static_cast<size_t>(-1);

	inline constexpr std::chrono::system_clock::time_point unknown_time{ std::chrono::system_clock::duration{0} };

	template<typename T>
	struct PageItem {
		T item;
		pageoff offset;
	};

	enum class RequestErrorCode {
		Unknown,
		NotImplemented,
		InvalidArguments,
		ServerError,
		InvalidCredentials,
	};

	struct RequestError {
		RequestErrorCode code;
		std::string message;
	};

	template<typename T, typename Error = RequestError>
	using Response = expected<T, Error>;

	inline auto make_response_error(RequestErrorCode code, std::string message) {
		return unexpected(RequestError{
			.code = code,
			.message = std::move(message)
		});
	}

	template<typename T>
	using NetworkRequestTask = asyncnet::NetworkTask<Response<T>>;

	template<typename T, typename Container = std::vector<PageItem<T>>>
	struct PageResults {
		Container results;
		pageoff next_offset = pageoff(0);
		size_t total_count = 0;
	};

	struct GetFilters {
		pageoff from    = 0;
		size_t limit    = page_no_limit;
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
	 * @brief Basic interval [from, to]
	 */
	template<typename T>
	struct Interval {
		/**
		 * Search context: No limit or start of interval
		 * Support check: Min value of interval
		 */
		std::optional<T> from;
		/**
		 * Search context: No limit or end of interval
		 * Support check: Max value of interval
		 */
		std::optional<T> to;
		/**
		 * Search context: Is interval exclusive (not containing in interval).
		 *                 For example exclusive interval [10, 20] => (-inf;10]U[20;+inf)
		 * Support check: Is exclusive supported
		 */
		bool exclusive = false;
	};

	/**
	 * @brief Integer interval
	 * Can be used for pages count, episodes count, etc.
	 */
	using IntInterval = Interval<std::ptrdiff_t>;

	/**
	 * @brief Time interval
	 * Can be used for update time, etc.
	 */
	using TimeInterval = Interval<std::chrono::system_clock::time_point>;

	/**
	 * @brief Relative time interval
	 * Counts time from now(). Can be used for update time, etc.
	 */
	using RelativeTimeInterval = Interval<std::chrono::system_clock::duration>;

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

	/**
	 * @brief Any type that can be used for search
	 * @see SearchRequestQuery
	 */
	using SearchItemVariant = std::variant<
		TextQuery,
		IntInterval,
		TimeInterval,
		RelativeTimeInterval,
		ItemSelection,
		Checkmark>;

	using SearchItems = std::map<std::string, SearchItemVariant, std::less<>>;

	/**
	 * Namespace for default keys, that can be used for search
	 * @see SearchRequestQuery
	 */
	namespace search_keys {
		/// Filter by @see Series. Usually TextQuery
		inline constexpr std::string_view series = "series";
		/// Filter by pages/episodes count. Usually DirectionalInterval/BidirectionalInterval
		inline constexpr std::string_view pages = "icount";
		inline constexpr std::string_view episodes = "icount";
		/// Filter by Tag. Usually TextQuery
		inline constexpr std::string_view tag = "tag";
		/// Filter by upload time (last time when item was updated on specific page). Usually DirectionalInterval/BidirectionalInterval
		inline constexpr std::string_view upload_time = "upd_time";
		/// Filter by release time (actual time when item was released). Usually DirectionalInterval/BidirectionalInterval
		inline constexpr std::string_view release_time = "rel_time";
		/// Filter by title, the text must be contained in title, but maybe not fully. Usually TextQuery
		inline constexpr std::string_view title = "title";
		/// Filter by status like "Announced", "Released" (Aired state of item). Usually ? (TextQuery)
		inline constexpr std::string_view status = "status";
		/// Filter by Rating. ? (DirectionalInterval/BidirectionalInterval)
		inline constexpr std::string_view rating = "rating";
		/// Filter by year. ? (DirectionalInterval/BidirectionalInterval)
		inline constexpr std::string_view year = "year";
		/// Filter by AgeRestriction. Usually DirectionalInterval/ItemSelection
		inline constexpr std::string_view age_restriction = "age_res";
	}

	/**
	 * @brief Any type that can be used for search
	 * @see SearchItemVariant
	 * @see search_keys
	 */
	struct SearchRequestQuery {
		std::string query;
		SearchItems filters;

		//template<typename Value>
		//SearchItemVariant& insert_filter(Value&& value) {

		//}
	};

	/**
	 * @brief Data used to serialize/deserialize "units" from getters
	 */
	struct SerializedGetterData {
		std::string url;
		std::map<std::string, std::string, std::less<>> params;
	};

	/**
	 * @brief Data for authentification using username and password
	 */
	struct AuthenticationUserPassword {
		std::string username;
		std::string password;
		bool requires_2fa = false;
	};

	/**
	 * @brief Data for authentification using token
	 *        (some string representing all required information
	 *        to identify user)
	 */
	struct AuthenticationToken {
		std::string token;
		std::string type;
	};

	/**
	 * @brief Data that can be used for authentification
	 */
	using AuthenticationData = std::variant<AuthenticationUserPassword, AuthenticationToken>;
}