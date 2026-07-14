/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/engines/booru/BooruPoolGetter.hpp"
#include "aniparse/engines/booru/BooruEngine.hpp"
#include "aniparse/engines/booru/BooruSite.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"

#include <boost/json.hpp>

#include <variant>

namespace aniparse::engines {

BooruPoolGetter::BooruPoolGetter(const BooruSite& site, ImageContainerID id)
    : site_(site), id_(id) {}

ImageContainerCompatibilities BooruPoolGetter::compatibilities() const noexcept {
	return {};
}

NetworkRequestTask<ImageContainerInfo> BooruPoolGetter::info(RequestorContext context) const {
	const BooruEngine& engine = site_.engine();
	std::string_view base = context.base_url(site_.api_hosts);
	GetRequest request = engine.pool_request(base, id_);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	// Pool metadata is a bare object today (the one family with pools returns its
	// pool endpoint as an object); a wrapping family would grow a hook.
	const boost::json::object* pool = json_result->if_object();
	if (!pool) {
		co_return make_response_error(RequestErrorCode::NotFound, "booru pool not found");
	}

	co_return engine.pool_to_container_info(*pool);
}

NetworkRequestTask<PageResults<ImageItem>> BooruPoolGetter::items(
    RequestorContext context, GetFilters filters) const {
	// The engine builds the pool-order page request (a listing in pool order), so
	// items() streams the collection without holding the whole id list.
	const BooruEngine& engine = site_.engine();
	std::string_view base = context.base_url(site_.api_hosts);
	GetRequest request = engine.pool_items_request(base, id_, filters);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	PageResults<ImageItem> results;
	const boost::json::array* posts = engine.posts_of(*json_result);
	if (!posts) {
		co_return results;
	}
	for (const boost::json::value& entry : *posts) {
		const boost::json::object* post = entry.if_object();
		if (!post) {
			continue;
		}
		// A banned/deleted post in the pool has no servable file — skip it rather
		// than yield a media-less item.
		if (std::optional<ImageItem> item = engine.post_to_item(*post, site_.media_referer)) {
			results.results.push_back(PageItem<ImageItem>{
			    .item = std::move(*item),
			    .offset = filters.from + static_cast<pageoff>(results.results.size()),
			});
		}
	}
	results.next_offset = filters.from + static_cast<pageoff>(results.results.size());
	co_return results;
}

NetworkRequestTask<SerializedGetterData> BooruPoolGetter::serialize() const {
	// "pools/" prefix distinguishes a pool from a bare post id on restore.
	co_return SerializedGetterData{ .url = "pools/" + std::to_string(id_) };
}

} // namespace aniparse::engines
