/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/engines/booru/BooruPoolGetter.hpp"
#include "aniparse/engines/booru/BooruEngine.hpp"
#include "aniparse/engines/booru/BooruSite.hpp"
#include "aniparse/runtime/RequestContext.hpp"
#include "aniparse/utility/Coroutines.hpp"

#include <boost/json.hpp>

#include <variant>

namespace aniparse::engines {

BooruPoolGetter::BooruPoolGetter(const BooruSite& site, ImageContainerID id)
    : site_(site), id_(id) {}

ImageContainerCapabilities BooruPoolGetter::capabilities() const noexcept {
	return {};
}

RequestTask<ImageContainerInfo> BooruPoolGetter::load_info(RequestContext context) const {
	const BooruEngine& engine = site_.engine();
	std::string_view base = context.base_url();
	GetRequest request = engine.pool_request(base, id_);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	co_return engine.pool_to_container_info(*json_result);
}

RequestTask<PageResults<ImageItem>> BooruPoolGetter::items(
    RequestContext context, GetFilters filters) const {
	// The engine builds the pool-order page request (a listing in pool order), so
	// items() streams the collection without holding the whole id list.
	const BooruEngine& engine = site_.engine();
	std::string_view base = context.base_url();
	GetRequest request = engine.pool_items_request(base, id_, filters);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	PageResults<ImageItem> results;
	auto posts = engine.parse_posts(*json_result, site_.media_referer);
	if (!posts)
		co_return unexpected(std::move(posts.error()));
	for (std::size_t index = 0; index < posts->size(); ++index) {
		if (auto& item = (*posts)[index].item) {
			results.results.push_back(PageItem<ImageItem>{
			    .item = std::move(*item), .offset = filters.from + static_cast<pageoff>(index)});
		}
	}
	results.next_offset = filters.from + static_cast<pageoff>(posts->size());
	co_return results;
}

RequestTask<SerializedGetterData> BooruPoolGetter::serialize() const {
	// "pools/" prefix distinguishes a pool from a bare post id on restore.
	co_return SerializedGetterData{ .url = "pools/" + std::to_string(id_) };
}

} // namespace aniparse::engines
