/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/images/Image.hpp"

#include <optional>

namespace aniparse::engines {

struct BooruSite;

/**
 * @brief One booru post, as an image container-of-one. Identity is the numeric post
 * id, round-tripped through serialize(). Generic over the site: the fetch host comes
 * from the injected @ref BooruSite and all per-family behaviour (request shape,
 * single-post extraction, field mapping) from its engine; the site outlives the getter
 * (a static instance).
 *
 * Built two ways. From a search/list result the whole post is already in hand — a
 * booru list response carries the same post objects the detail endpoint returns, run
 * through the same mapping — so preview_info()/items() answer from it with no request.
 * From a URL or a serialized id the getter holds identity only, and those methods
 * fetch the post.
 *
 * Nothing is written after construction (@see ImageContainerGetter): info() always
 * asks the source, and a fetched post is mapped into the local result rather than
 * kept in a member. A getter built from an id therefore pays one request per call —
 * the duplicate belongs to the response cache to remove, not to per-getter state,
 * which would make info() answer forever from whatever it happened to see first.
 */
class BooruContainerGetter : public ImageContainerGetter {
public:
	/// @param site The site descriptor (host + engine) — must outlive this getter.
	/// @param id   The numeric post id — its identity.
	/// @param info Container metadata already mapped from a list result; nullopt when
	///             constructed from an id alone. Its presence is what marks the post
	///             as supplied up front.
	/// @param item The post's media leaf, mapped alongside @p info; nullopt both when
	///             the post was not supplied and when it has no servable file.
	BooruContainerGetter(const BooruSite& site, ImageContainerID id,
	                     std::optional<ImageContainerInfo> info = std::nullopt,
	                     std::optional<ImageItem> item = std::nullopt);

	ImageContainerCompatibilities compatibilities() const noexcept override;

	/// The list result this getter was built from, or nullopt when it was built from
	/// an id alone. Free — a booru list response carries whole posts, so a getter out
	/// of search needs no request to fill a card.
	[[nodiscard]] std::optional<ImageContainerInfo> preview_info() const noexcept override;

	NetworkRequestTask<ImageContainerInfo> info(RequestorContext context) const override;

	NetworkRequestTask<PageResults<ImageItem>> items(
	    RequestorContext context,
	    GetFilters filters) const override;

	NetworkRequestTask<SerializedGetterData> serialize() const override;

private:
	/// The post as this engine maps it: the container metadata and its media leaf
	/// (absent when the post serves no file). Both come out of one response.
	struct Post {
		ImageContainerInfo info;
		std::optional<ImageItem> item;
	};

	/// Fetch and map the post. Returns the mapping — writes nothing.
	NetworkRequestTask<Post> fetch_post(RequestorContext& context) const;

	const BooruSite& site_;
	const ImageContainerID id_;
	/// The list result, when this getter came from one — set once, read-only.
	const std::optional<ImageContainerInfo> info_;
	const std::optional<ImageItem> item_;
};

} // namespace aniparse::engines
