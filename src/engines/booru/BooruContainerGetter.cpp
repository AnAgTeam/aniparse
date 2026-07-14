/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/engines/booru/BooruContainerGetter.hpp"
#include "aniparse/engines/booru/BooruEngine.hpp"
#include "aniparse/engines/booru/BooruSite.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"

#include <boost/json.hpp>

#include <variant>

namespace aniparse::engines {

BooruContainerGetter::BooruContainerGetter(const BooruSite& site, ImageContainerID id,
                                           std::optional<ImageContainerInfo> info,
                                           std::optional<ImageItem> item)
    : site_(site), id_(id), info_(std::move(info)), item_(std::move(item)) {}

ImageContainerCompatibilities BooruContainerGetter::compatibilities() const noexcept {
	return {};
}

NetworkRequestTask<BooruContainerGetter::Post> BooruContainerGetter::fetch_post(
    RequestorContext& context) const {
	const BooruEngine& engine = site_.engine();
	std::string_view base = context.base_url(site_.api_hosts);
	GetRequest request = engine.container_request(base, id_);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	const boost::json::object* post = engine.single_post(*json_result);
	if (!post) {
		co_return make_response_error(RequestErrorCode::NotFound, "booru post not found");
	}

	co_return Post{
		.info = engine.post_to_container_info(*post, site_.media_referer),
		.item = engine.post_to_item(*post, site_.media_referer),
	};
}

std::optional<ImageContainerInfo> BooruContainerGetter::preview_info() const noexcept {
	return info_;
}

NetworkRequestTask<ImageContainerInfo> BooruContainerGetter::info(RequestorContext context) const {
	// Always asks the source, even when a list result is in hand: info() is the fresh
	// record, and the card this getter was built with may be minutes or days old.
	auto post = co_await fetch_post(context);
	if (!post) {
		co_return unexpected(std::move(post.error()));
	}
	co_return std::move(post->info);
}

NetworkRequestTask<PageResults<ImageItem>> BooruContainerGetter::items(
    RequestorContext context, GetFilters) const {
	// A post is a container-of-one: at most a single media leaf, ignoring paging.
	auto page_of = [](std::optional<ImageItem> item) {
		PageResults<ImageItem> page;
		if (item) {
			page.results.push_back(PageItem<ImageItem>{ .item = *std::move(item), .offset = 0 });
			page.total_count = 1;
			page.next_offset = 1;
		}
		return page;
	};

	// Built from a listing: the post came with it, so answer from what the constructor
	// set. Built from an id: fetch. Either way nothing is written back.
	if (info_) {
		co_return page_of(item_);
	}

	auto post = co_await fetch_post(context);
	if (!post) {
		co_return unexpected(std::move(post.error()));
	}
	co_return page_of(std::move(post->item));
}

NetworkRequestTask<SerializedGetterData> BooruContainerGetter::serialize() const {
	// Identity is the post id; the cached info/item are a fetch-time convenience, not
	// identity, so a restored getter re-resolves via the engine's container request.
	co_return SerializedGetterData{ .url = std::to_string(id_) };
}

} // namespace aniparse::engines
