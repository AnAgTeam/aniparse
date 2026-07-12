/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/engines/BooruImagesGetter.hpp"
#include "aniparse/engines/BooruContainerGetter.hpp"
#include "aniparse/engines/BooruEngine.hpp"
#include "aniparse/engines/BooruPoolGetter.hpp"
#include "aniparse/engines/BooruSite.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"

#include <boost/json.hpp>

#include <cstdlib>
#include <memory>
#include <optional>

namespace aniparse::engines {

namespace {
	/// Parse a list/search envelope into a page of container getters, each carrying
	/// its mapped info + media leaf so info()/items() need no refetch. The engine
	/// unwraps the envelope (bare array vs wrapped) and maps each post; each getter is
	/// bound to the same site.
	PageResults<std::unique_ptr<ImageContainerGetter>> build_post_page(
	    const BooruSite& site, const boost::json::value& envelope,
	    const GetFilters& filters) {
		PageResults<std::unique_ptr<ImageContainerGetter>> results;
		const BooruEngine& engine = site.engine();
		const boost::json::array* posts = engine.posts_of(envelope);
		if (!posts) {
			return results;
		}
		for (const boost::json::value& entry : *posts) {
			const boost::json::object* post = entry.if_object();
			if (!post) {
				continue;
			}
			auto id = static_cast<ImageContainerID>(aniparse::json::integer(*post, "id"));
			if (id == invalid_image_container_id) {
				continue;
			}
			results.append(filters.from, std::make_unique<BooruContainerGetter>(
			    site, id,
			    engine.post_to_container_info(*post, site.media_referer),
			    engine.post_to_item(*post, site.media_referer)));
		}
		return results;
	}
} // namespace

NetworkRequestTask<SearchCompatibilities> BooruImagesGetter::search_support(RequestorContext) {
	co_return site_.engine().search_support();
}

NetworkRequestTask<std::vector<SearchSuggestion>> BooruImagesGetter::suggest(
    RequestorContext context, std::string partial, std::optional<std::string> kind) {
	const BooruEngine& engine = site_.engine();
	std::string_view base = context.base_url(site_.api_hosts);
	GetRequest request = engine.suggest_request(base, std::move(partial), std::move(kind));
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return engine.parse_suggestions(*json_result);
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> BooruImagesGetter::search(
    RequestorContext context, SearchRequestQuery query, GetFilters filters) {
	auto support = co_await search_support(context);
	if (!support) {
		co_return unexpected(std::move(support.error()));
	}
	if (auto errors = validate_query(*support, query, filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	std::string_view base = context.base_url(site_.api_hosts);
	GetRequest request = site_.engine().list_request(base, query.query, filters);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return build_post_page(site_, *json_result, filters);
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> BooruImagesGetter::latest(
    RequestorContext context, GetFilters filters) {
	if (auto errors = validate_latest_filters(filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	// No tags = the source's default newest-first feed.
	std::string_view base = context.base_url(site_.api_hosts);
	GetRequest request = site_.engine().list_request(base, std::string{}, filters);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return build_post_page(site_, *json_result, filters);
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> BooruImagesGetter::parse_url(
    RequestorContext, ParsedUrl url) {
	const BooruEngine& engine = site_.engine();
	// A pool URL is a container-of-many; a post URL is a container-of-one.
	if (engine.supports_pools()) {
		if (std::optional<ImageContainerID> pool = engine.pool_id_from_url(url)) {
			co_return std::make_unique<BooruPoolGetter>(site_, *pool);
		}
	}
	if (std::optional<ImageContainerID> post = engine.post_id_from_url(url)) {
		co_return std::make_unique<BooruContainerGetter>(site_, *post);
	}
	co_return make_response_error(RequestErrorCode::InvalidArguments,
	                              "URL addresses no booru post or pool");
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> BooruImagesGetter::from_serialized(
    SerializedGetterData data) {
	// Inverse of the container/pool getters' serialize(): a bare number is a post id;
	// a "pools/"-prefixed value is a pool id (only ever produced by a pool getter, so
	// only routed back to one for a family that has pools).
	if (data.url.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "serialized booru getter carries no id");
	}
	constexpr std::string_view pool_prefix = "pools/";
	if (site_.engine().supports_pools() && data.url.starts_with(pool_prefix)) {
		auto id = static_cast<ImageContainerID>(std::atol(data.url.c_str() + pool_prefix.size()));
		co_return std::make_unique<BooruPoolGetter>(site_, id);
	}
	auto id = static_cast<ImageContainerID>(std::atol(data.url.c_str()));
	co_return std::make_unique<BooruContainerGetter>(site_, id);
}

} // namespace aniparse::engines
