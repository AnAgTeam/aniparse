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
    : site_(site), id_(id), info_(std::move(info)), item_(std::move(item)),
      loaded_(info_.has_value()) {}

ImageContainerCompatibilities BooruContainerGetter::compatibilities() const noexcept {
	return {};
}

NetworkRequestTask<std::monostate> BooruContainerGetter::ensure_loaded(RequestorContext& context) {
	if (loaded_) {
		co_return std::monostate{};
	}

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

	info_   = engine.post_to_container_info(*post, site_.media_referer);
	item_   = engine.post_to_item(*post, site_.media_referer);
	loaded_ = true;
	co_return std::monostate{};
}

NetworkRequestTask<ImageContainerInfo> BooruContainerGetter::info(RequestorContext context) {
	if (auto loaded = co_await ensure_loaded(context); !loaded) {
		co_return unexpected(std::move(loaded.error()));
	}
	co_return *info_;
}

NetworkRequestTask<PageResults<ImageItem>> BooruContainerGetter::items(
    RequestorContext context, GetFilters) {
	if (auto loaded = co_await ensure_loaded(context); !loaded) {
		co_return unexpected(std::move(loaded.error()));
	}

	// A post is a container-of-one: at most a single media leaf, ignoring paging.
	PageResults<ImageItem> page;
	if (item_) {
		page.results.push_back(PageItem<ImageItem>{ .item = *item_, .offset = 0 });
		page.total_count = 1;
		page.next_offset = 1;
	}
	co_return page;
}

NetworkRequestTask<SerializedGetterData> BooruContainerGetter::serialize() {
	// Identity is the post id; the cached info/item are a fetch-time convenience, not
	// identity, so a restored getter re-resolves via the engine's container request.
	co_return SerializedGetterData{ .url = std::to_string(id_) };
}

} // namespace aniparse::engines
