/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
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

	constexpr int unknown_manga_chapters = -1;
	constexpr MangaTranslationID any_manga_translation = -1;

	struct MangaInfo {
		MangaID id = MangaID(0);

		std::string title;
		std::optional<std::string> original_title;
		AttributedText description;

		std::chrono::system_clock::time_point update_time = unknown_time;
		std::chrono::system_clock::time_point release_time = unknown_time;
		AiredStatus status;
		
		RelatedUser author;
		RelatedUser artist;
		std::optional<Series> series;

		std::vector<Image> previews;
		std::vector<Tag> tags;

		Rating rating;
		std::optional<ViewStats> views;
		std::optional<UserList> user_lists;
		AgeRestriction age_restriction = 0;

		std::optional<RelatedUser> uploader;

		int total_chapters = unknown_manga_chapters;

		bool is_hentai = false;
	};

	struct MangaTranslationInfo {
		MangaTranslationID id;
		std::string language;
		RelatedUser translator;
	};

	struct MangaChapterInfo {
		int volume = 0;
		int chapter = 0;
		std::string name;
		std::string description;
		std::vector<Image> previews;
		std::chrono::system_clock::time_point update_time = unknown_time;
		std::chrono::system_clock::time_point release_time = unknown_time;
	};

	struct MangaPage {
		Image image;
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
		size_t alt_links_count;
		CompatibilitiesFlags flags = compatibilities_flags::default_flags;
	};

	struct MangaGetterRootCompatibilities {
		FilteringFlags filtering_support = filtering_flags::default_flags;
		CompatibilitiesFlags compatibilities = compatibilities_flags::default_flags;
	};

	using MangaChapterInfoPaginator = ForwardPaginator<std::unique_ptr<MangaChapterGetter>>;

	struct MangaGetter {

		MangaGetter(RequestorContext context);

		virtual ~MangaGetter() = default;

		virtual MangaGetterCompatibilities compatibilies() const = 0;

		virtual asyncnet::NetworkTask<MangaInfo> preview_info();

		virtual asyncnet::NetworkTask<MangaInfo> info() = 0;

		virtual asyncnet::NetworkTask<std::vector<MangaTranslationInfo>> translation_info(GetFilters filters);

		virtual asyncnet::NetworkTask<PageResults<MangaChapterInfo>> chapters_info(
			GetFilters filters,
			MangaTranslationID translation = any_manga_translation);

		virtual asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> related(GetFilters filters);

		virtual asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> similar(GetFilters filters);

		virtual asyncnet::NetworkTask<PageResults<MangaPage>> chapter_pages(
			int volume, int chapter,
			GetFilters filters,
			MangaTranslationID translation = any_manga_translation) = 0;

		virtual void reset();

		virtual bool update_context(RequestorContext context);

	protected:
		RequestorContext context;
	};

	struct MangaRootGetter {
		virtual ~MangaRootGetter() = default;

		virtual MangaGetterRootCompatibilities search_support() const;
		virtual MangaGetterRootCompatibilities latest_support() const;

		/**
		 * @todo
		 * Search mangas with query and/or filters (advanced query may come as filters)
		 * By default throws NotImplementedError
		 * @param context Client to perform HTTP requests
		 * @param query Query string, plain text
		 * @param filters Filters to apply to results (e.g. sort ...)
		 * @throw NotImplementedError If the method isn't implemented by the parser
		 * @return ...
		 */
		virtual asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> search(
			RequestorContext context,
			std::string query,
			GetFilters filters);

		/**
		 * @todo
		 * Get latest parser source released mangas
		 * By default throws NotImplementedError
		 * @param context Client to perform HTTP requests
		 * @param filters Filters to apply to results (e.g. sort ...)
		 * @throw NotImplementedError If the method isn't implemented by the parser
		 * @return ...
		 */
		virtual asyncnet::NetworkTask<PageResults<std::unique_ptr<MangaGetter>>> latest(
			RequestorContext context,
			GetFilters filters);

		/**
		 * @see MangaGetter
		 * Parse the url and return corresponding getter
		 * By default throws NotImplementedError
		 * @param context Client to perform HTTP requests
		 * @param url The url to parse
		 * @throw NotImplementedError If the method isn't implemented by the parser
		 * @return Task to get MangaGetter
		 */
		virtual asyncnet::NetworkTask<std::unique_ptr<MangaGetter>> parse_url(
			RequestorContext context, 
			std::string url);

	protected:
		GetterContext context;
	};
}