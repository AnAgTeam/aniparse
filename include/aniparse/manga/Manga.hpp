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

		std::optional<std::vector<Tag>> tags;

		Rating rating;
		std::optional<ViewStats> views;
		std::optional<UserList> user_lists;
		AgeRestriction age_restriction;

		std::optional<RelatedUser> uploader;

		int total_chapters;

		bool is_hentai;
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

	struct MangaChapterGetter {

	};

	struct MangaGetterCompatibilities {
		CompatibilitiesFlags flags;
	};

	struct MangaGetter {

		virtual ~MangaGetter() = default;

		virtual MangaGetterCompatibilities compatibilies() const = 0;

		virtual asyncnet::NetworkTask<MangaInfo> info();

		virtual asyncnet::NetworkTask<std::vector<MangaTranslationInfo>> translation_info(GetFilters filters);

		virtual asyncnet::NetworkTask<std::vector<MangaChapterInfo>> chapters_info(GetFilters filters);

		virtual asyncnet::NetworkTask<MangaGetterPaginator> similar();

		virtual bool update_client(std::shared_ptr<ClientContext> new_client);

	protected:
		std::shared_ptr<ClientContext> client;
	};

	struct ImagesGetter {
		virtual ~ImagesGetter() = default;

		virtual asyncnet::NetworkTask<MangaGetterPaginator> search(std::shared_ptr<ClientContext> client, std::string query, GetFilters filters);

		virtual asyncnet::NetworkTask<MangaGetterPaginator> latest(std::shared_ptr<ClientContext> client, GetFilters filters);

	protected:
		GetterContext context;
	};
}