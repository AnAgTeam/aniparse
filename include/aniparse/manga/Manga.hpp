#pragma once
#include "aniparse/Common.hpp"

namespace aniparse {

	using MangaID = int;
	using MangaTranslationID = int;

	struct MangaGetter;

	using MangaGetterPaginator = ForwardPaginator<std::unique_ptr<MangaGetter>>;

	enum class DefaultMangaType {
		Manga,
		Dojinshi,
		Manhwa,
		Manhua,
		Other
	};

	struct MangaType {
		MangaType(DefaultMangaType type);

		std::string name;
	};

	struct MangaInfo {
		MangaID id;

		std::string title;
		std::optional<std::string> original_title;
		std::string description;

		std::chrono::system_clock::time_point update_time;
		std::chrono::system_clock::time_point release_time;
		AiredStatus status;
		
		RelatedUser author;
		RelatedUser artist;
		std::optional<Series> series;

		std::vector<Image> previews;
		std::vector<Tag> tags;

		Rating rating;
		std::optional<ViewStats> views;
		std::optional<UserList> user_lists;
		AgeRestriction age_restriction;

		std::optional<RelatedUser> uploader;

		int total_chapters = 0;

		bool is_hentai = false;
	};

	struct MangaTranslationInfo {
		MangaTranslationID id;
		std::string language;
		std::string translator;
	};

	struct MangaChapterInfo {
		int volume;
		int chapter;
		std::string name;
		std::chrono::system_clock::time_point update_time;
		std::chrono::system_clock::time_point release_time;
	};

	struct MangaChapterGetter : ForwardPaginator<Image> {

		virtual asyncnet::NetworkTask<std::vector<Image>> next() = 0;
		virtual bool end() const = 0;
		virtual asyncnet::NetworkTask<size_t> total_items() = 0;

		virtual bool update_context(RequestorContext context);

	protected:
		RequestorContext context;
	};

	struct MangaGetterCompatibilities {
		CompatibilitiesFlags flags;
	};

	using MangaChapterInfoPaginator = ForwardPaginator<std::unique_ptr<MangaChapterGetter>>;

	struct MangaGetter {

		MangaGetter(RequestorContext context);

		virtual ~MangaGetter() = default;

		virtual MangaGetterCompatibilities compatibilies() const = 0;

		virtual asyncnet::NetworkTask<MangaInfo> info() = 0;

		virtual asyncnet::NetworkTask<std::vector<MangaTranslationInfo>> translation_info(GetFilters filters);

		virtual asyncnet::NetworkTask<std::vector<MangaChapterInfo>> chapters_info(GetFilters filters);

		virtual asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> similar();

		virtual asyncnet::NetworkTask<std::unique_ptr<MangaChapterGetter>> get_chapter(int volume, int chapter) = 0;

		virtual bool update_context(RequestorContext context);

	protected:
		RequestorContext context;
	};

	struct MangaRootGetter {
		virtual ~MangaRootGetter() = default;

		/**
		 * @see MangaGetterPaginator
		 * Search mangas with query and/or filters (advanced query may come as filters)
		 * By default throws NotImplementedError
		 * @param context Client to perform HTTP requests
		 * @param query Query string, plain text
		 * @param filters Filters to apply to results (e.g. sort)
		 * @throw NotImplementedError If the method isn't implemented by the parser
		 * @return Task to get MangaGetterPaginator
		 */
		virtual asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> search(RequestorContext context, std::string query, GetFilters filters);

		/**
		 * @see MangaGetterPaginator
		 * Get latest parser source released mangas
		 * By default throws NotImplementedError
		 * @param context Client to perform HTTP requests
		 * @param filters Filters to apply to results (e.g. sort)
		 * @throw NotImplementedError If the method isn't implemented by the parser
		 * @return Task to get MangaGetterPaginator
		 */
		virtual asyncnet::NetworkTask<std::unique_ptr<MangaGetterPaginator>> latest(RequestorContext context, GetFilters filters);

		/**
		 * @see MangaGetter
		 * Parse the url and return corresponding getter
		 * By default throws NotImplementedError
		 * @param context Client to perform HTTP requests
		 * @param url The url to parse
		 * @throw NotImplementedError If the method isn't implemented by the parser
		 * @return Task to get MangaGetter
		 */
		virtual asyncnet::NetworkTask<std::unique_ptr<MangaGetter>> parse_url(RequestorContext context, std::string_view url);

	protected:
		GetterContext context;
	};
}