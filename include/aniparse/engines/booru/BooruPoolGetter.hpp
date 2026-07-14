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
 * @brief A booru pool, as an image container-of-many: an ordered set of posts (a
 * scanned set, a doujin, ...). Identity is the numeric pool id, round-tripped through
 * serialize() with a "pools/" prefix to distinguish it from a post. Generic over the
 * site: the fetch host comes from the injected @ref BooruSite and pool metadata /
 * pool-order paging from its engine. Only constructed for families whose engine
 * supports_pools().
 *
 * info() resolves the pool's metadata (name, description, post_count); items() pages
 * through the pool's posts in order, mapping each to a media leaf — the
 * container-of-many counterpart of a single post's container-of-one.
 */
class BooruPoolGetter : public ImageContainerGetter {
public:
	/// @param site The site descriptor (host + engine) — must outlive this getter.
	/// @param id   The numeric pool id — its identity.
	BooruPoolGetter(const BooruSite& site, ImageContainerID id);

	ImageContainerCompatibilities compatibilities() const noexcept override;

	NetworkRequestTask<ImageContainerInfo> info(RequestorContext context) const override;

	NetworkRequestTask<PageResults<ImageItem>> items(
	    RequestorContext context,
	    GetFilters filters) const override;

	NetworkRequestTask<SerializedGetterData> serialize() const override;

private:
	const BooruSite& site_;
	const ImageContainerID id_;
};

} // namespace aniparse::engines
