/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Common.hpp"

#include <coro/expected.hpp>

namespace aniparse {

using MangaID            = int;
using MangaTranslationID = int;

struct MangaGetter;

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

inline constexpr MangaID invalid_manga_id                 = MangaID{ 0 };
inline constexpr MangaTranslationID any_manga_translation = -1;

struct MangaInfo {
	MangaID id = invalid_manga_id;

	std::string title;
	std::optional<std::string> original_title;
	AttributedText description;

	std::chrono::system_clock::time_point update_time  = unknown_time;
	std::chrono::system_clock::time_point release_time = unknown_time;
	AiredStatus status;

	RelatedUser author;
	RelatedUser artist;
	std::optional<Series> series;

	std::vector<Image> previews;
	std::vector<Tag> tags;

	std::optional<Rating> rating;
	std::optional<ViewStats> views;
	std::optional<UserList> user_lists;
	AgeRestriction age_restriction = 0;

	std::optional<RelatedUser> uploader;

	std::optional<long> total_chapters;

	bool is_hentai = false;
};

struct MangaTranslationInfo {
	MangaTranslationID id;
	std::string language;
	RelatedUser translator;
};

struct MangaChapterInfo {
	long volume  = 0;
	long chapter = 0;
	std::string name;
	std::string description;
	std::vector<Image> previews;
	std::chrono::system_clock::time_point update_time  = unknown_time;
	std::chrono::system_clock::time_point release_time = unknown_time;
};

struct MangaPage {
	Image image;
};

/// A selectable mirror (base URL) for a source. Selected by index via
/// RequestorContext::alt_link(). Kept as a struct so per-mirror metadata can be
/// added later without changing the collection type.
struct AltLink {
	std::string url;
};

struct MangaGetterCompatibilities {
	std::vector<AltLink> alt_links;
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};

struct MangaGetterRootCompatibilities {
	FilteringFlags filtering_support     = filtering_flags::default_flags;
	CompatibilitiesFlags compatibilities = compatibilities_flags::default_flags;
};

struct SearchCompatibilities {
	SearchItems supported_filters;
	FilteringFlags filtering_support     = filtering_flags::default_flags;
	CompatibilitiesFlags compatibilities = compatibilities_flags::default_flags;
};

/**
 * @brief Interface for getting one specific manga information.
 */
struct MangaGetter {
	virtual ~MangaGetter() = default;

	virtual MangaGetterCompatibilities compatibilies() const noexcept = 0;

	virtual NetworkRequestTask<MangaInfo> preview_info(RequestorContext context) noexcept;

	virtual NetworkRequestTask<MangaInfo> info(RequestorContext context) noexcept = 0;

	virtual NetworkRequestTask<PageResults<MangaTranslationInfo>> translation_info(
	    RequestorContext context,
	    GetFilters filters) noexcept;

	virtual NetworkRequestTask<PageResults<MangaChapterInfo>> chapters_info(
	    RequestorContext context,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation = std::nullopt) noexcept;

	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> related(
	    RequestorContext context,
	    GetFilters filters) noexcept;

	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> similar(
	    RequestorContext context,
	    GetFilters filters) noexcept;

	virtual NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
	    RequestorContext context,
	    int volume,
	    int chapter,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation = std::nullopt) noexcept = 0;

	virtual void reset() noexcept;

	virtual NetworkRequestTask<SerializedGetterData> serialize() noexcept = 0;
};

/**
 * @brief Root interface for manga. Used to get pages or general information
 */
struct MangaRootGetter {
	virtual ~MangaRootGetter() = default;

	virtual SearchCompatibilities search_support() const noexcept;
	virtual MangaGetterRootCompatibilities latest_support() const noexcept;

	/**
	 * @todo !
	 * @brief Request authentification with given data for parser service.
	 */
	virtual NetworkRequestTask<std::shared_ptr<const ParserConfig>> authenticate_context(
	    RequestorContext context,
	    AuthenticationData data) noexcept;

	/**
	 * @todo !

	 * @note By default passed client is forwarded.
	 */
	virtual std::shared_ptr<ParserConfig> default_config_from(std::shared_ptr<const ParserConfig> base_config) const;

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
	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters) noexcept;

	/**
	 * @todo
	 * Get latest parser source released mangas
	 * By default throws NotImplementedError
	 * @param context Client to perform HTTP requests
	 * @param filters Filters to apply to results (e.g. sort ...)
	 * @throw NotImplementedError If the method isn't implemented by the parser
	 * @return ...
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters) noexcept;

	/**
	 * @see MangaGetter
	 * Parse the url and return corresponding getter
	 * By default throws NotImplementedError
	 * @param context Client to perform HTTP requests
	 * @param url The url to parse
	 * @throw NotImplementedError If the method isn't implemented by the parser
	 * @return Task to get MangaGetter
	 */
	virtual NetworkRequestTask<std::unique_ptr<MangaGetter>> parse_url(
	    RequestorContext context,
	    std::string url) noexcept;

	/**
	 * @brief Getter for serialized data from one of serialize() methods
	 */
	virtual NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(SerializedGetterData data) noexcept = 0;

protected:
	GetterContext context;
};
} // namespace aniparse