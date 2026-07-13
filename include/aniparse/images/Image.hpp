/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Common.hpp"
#include "aniparse/ParsedUrl.hpp"

namespace aniparse {

using ImageContainerID = int;
inline constexpr ImageContainerID invalid_image_container_id = ImageContainerID{ 0 };

struct ImageContainerGetter;

/**
 * @brief What kind of media one item in a container is. A booru-style source
 * mixes stills and video in the same tagged catalog, so the leaf declares its
 * own kind instead of assuming "still". @see ImageItem
 */
enum class ImageItemKind {
	Still,    ///< A single raster image (jpg/png/webp).
	Animated, ///< An animated image with no audio track (gif/animated webp).
	Video,    ///< A video clip with its own container/codec (webm/mp4).
};

/**
 * @brief One media element of a container: the fetchable resource plus what it
 * is. The leaf of the images domain, mirroring MangaPage. A flat single-media
 * source (a booru post) yields one; a gallery yields many.
 */
struct ImageItem {
	/// The fetch descriptor — url + headers + resolution. @see Image
	Image image;
	ImageItemKind kind = ImageItemKind::Still;
	/// Playback length for Animated/Video; absent for stills or when unknown.
	std::optional<std::chrono::milliseconds> duration;
	/// A poster/thumbnail distinct from the media itself, when the source offers
	/// one (typically a video's still frame); empty otherwise.
	std::optional<Image> poster;
};

/**
 * @brief Metadata of one container — a single booru post or a whole gallery.
 * The images counterpart of MangaInfo: a plain data model returned by
 * ImageContainerGetter::info, not a bag of getter methods.
 */
struct ImageContainerInfo {
	ImageContainerID id = invalid_image_container_id;

	std::string title;
	AttributedText description;
	std::vector<Tag> tags;

	std::optional<RelatedUser> uploader;
	std::optional<Series> series;

	/// Cover/sample images for the container as a whole (grid thumbnails).
	std::vector<Image> previews;

	std::optional<Rating> rating;
	std::optional<ViewStats> views;

	std::chrono::system_clock::time_point update_time  = unknown_time;
	std::chrono::system_clock::time_point release_time = unknown_time;

	/// Opaque change marker for the whole container; @see MangaInfo::revision.
	std::string revision;

	AgeRestriction age_restriction = 0;

	/// How many ImageItems the container holds, when the source states it up
	/// front (a booru post is 1); absent when only discoverable by paging items().
	std::optional<long> total_items;

	bool is_hentai = false;
};

struct ImageContainerCompatibilities {
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};

struct ImagesGetterRootCompatibilities {
	SupportedSorts supported_sorts;
	CompatibilitiesFlags compatibilities = compatibilities_flags::default_flags;
};

/**
 * @brief One container of media: a single booru post or a whole gallery.
 * The images counterpart of MangaGetter.
 *
 * A getter is IMMUTABLE once constructed: its methods read what the constructor set and
 * never write it. Data known up front goes in the constructor; data fetched at request
 * time goes in RequestorContext::resources(), never into a member. This is what makes
 * concurrent calls on one getter safe without a lock, and callers rely on it.
 * @see MangaGetter for the full statement of the rule.
 */
struct ImageContainerGetter {
	virtual ~ImageContainerGetter() = default;

	virtual ImageContainerCompatibilities compatibilities() const noexcept = 0;

	/// Cheap, possibly-partial info for a list card; may skip fields info()
	/// fills. Defaults to info().
	virtual NetworkRequestTask<ImageContainerInfo> preview_info(RequestorContext context);

	virtual NetworkRequestTask<ImageContainerInfo> info(RequestorContext context) = 0;

	/**
	 * @brief The media items of this container, paginated via @p filters.
	 * A flat single-media source returns one item; a gallery pages through
	 * many. Mirrors MangaGetter::chapter_pages, minus the chapter ref — a
	 * container addresses its own media directly.
	 */
	virtual NetworkRequestTask<PageResults<ImageItem>> items(
	    RequestorContext context,
	    GetFilters filters) = 0;

	/**
	 * @brief User comments on the container, paginated via @p filters.
	 * Available when the parser advertises supports_commenting; NotImplemented
	 * by default. @see MangaGetter::comments
	 */
	virtual NetworkRequestTask<PageResults<Comment>> comments(
	    RequestorContext context,
	    GetFilters filters);

	virtual NetworkRequestTask<SerializedGetterData> serialize() = 0;
};

/**
 * @brief Root interface for an image source: search and browse containers.
 * The images counterpart of MangaRootGetter.
 */
struct ImagesGetter {
	virtual ~ImagesGetter() = default;

	/**
	 * @brief The search filters and sorts this source supports.
	 * Cache-first and async; validate a query against it with the free
	 * validate_query. @see MangaRootGetter::search_support
	 */
	virtual NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext context);
	virtual ImagesGetterRootCompatibilities latest_support() const noexcept;

	/**
	 * @brief Check the requested sort against this getter's own latest_support().
	 * Same contract as validate_query, for latest().
	 * @return Empty if the filters are valid; otherwise the violation.
	 */
	[[nodiscard]] std::vector<SearchQueryError> validate_latest_filters(const GetFilters& filters) const;

	/**
	 * @brief Search containers by query and/or filters (e.g. tags on a booru).
	 * NotImplemented by default.
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters);

	/**
	 * @brief Latest containers from the source. NotImplemented by default.
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters);

	/**
	 * @brief Autocomplete: suggest search tokens for a partial input.
	 * Advertised via SearchCompatibilities::compatibilities (supports_suggestions);
	 * NotImplemented by default. @p kind optionally narrows to one search axis — a
	 * search key (@see search_keys), the same vocabulary as the source's filters and
	 * as SearchSuggestion::category — best-effort, the source may ignore it. The
	 * returned SearchSuggestion::value is a search token that drops straight back
	 * into search (as that filter's value or a query token).
	 * @param context Client to perform HTTP requests
	 * @param partial The token being typed.
	 * @param kind Optional search axis to complete; nullopt = across the default axis.
	 * @return Suggestions, most relevant first.
	 */
	virtual NetworkRequestTask<std::vector<SearchSuggestion>> suggest(
	    RequestorContext context,
	    std::string partial,
	    std::optional<std::string> kind = std::nullopt);

	/**
	 * @brief Parse a url into the container getter it addresses.
	 * NotImplemented by default. @see MangaRootGetter::parse_url
	 */
	virtual NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> parse_url(
	    RequestorContext context,
	    ParsedUrl url);

	/**
	 * @brief Getter for serialized data from one of serialize() methods.
	 */
	virtual NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> from_serialized(SerializedGetterData data) = 0;
};

} // namespace aniparse
