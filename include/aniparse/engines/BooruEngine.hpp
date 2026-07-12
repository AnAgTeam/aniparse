/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Headers.hpp"
#include "aniparse/images/Image.hpp"
#include "aniparse/types/Request.hpp"

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aniparse::engines {

/**
 * @brief The engine dialect of one booru software family (a REST post-index flavour,
 * a query-CGI flavour, ...). Pure request-building and response-mapping — no network,
 * no coroutines, and no host resolution: the generic getters own the co_await and
 * resolve the fetch host from the site descriptor, passing the ready base URL into
 * each builder here. So an engine is host-agnostic, which is what lets two sites
 * running the same software share one engine and differ only in their @ref BooruSite
 * data.
 *
 * This is the seam that lets the three getter roles stay one implementation while the
 * per-family behaviour varies: endpoint grammar, envelope shape, id addressing, tag/sort
 * dialect and field mapping are all methods here. An engine is stateless; a single
 * static instance is shared by every site of a family.
 */
struct BooruEngine {
	virtual ~BooruEngine() = default;

	// --- config -----------------------------------------------------------------

	/// Headers every request carries (a descriptive User-Agent, at minimum). A
	/// family-level default; the host is site data (@see BooruSite::api_hosts).
	[[nodiscard]] virtual Headers api_headers() const = 0;

	/// Whether this family exposes pools (an ordered container-of-many). Gates the
	/// pool branch of URL routing and the use of BooruPoolGetter. Default: no pools.
	[[nodiscard]] virtual bool supports_pools() const noexcept { return false; }

	// --- search / listing -------------------------------------------------------

	/// The sorts and suggestion capabilities this source advertises (no network).
	[[nodiscard]] virtual SearchCompatibilities search_support() const = 0;

	/// A post-list request against @p base for @p tags with @p filters paging applied;
	/// a requested sort is folded into the tag string as this family's sort metatag.
	[[nodiscard]] virtual GetRequest list_request(std::string_view base,
	                                              std::string tags,
	                                              const GetFilters& filters) const = 0;

	/// The post array of a list/search response, or nullptr on zero results.
	[[nodiscard]] virtual const boost::json::array* posts_of(
	    const boost::json::value& envelope) const = 0;

	// --- single post (container-of-one) -----------------------------------------

	/// A single-post fetch request against @p base, addressed by id.
	[[nodiscard]] virtual GetRequest container_request(std::string_view base,
	                                                   ImageContainerID id) const = 0;

	/// The one post object of a single-post response, or nullptr when absent.
	[[nodiscard]] virtual const boost::json::object* single_post(
	    const boost::json::value& envelope) const = 0;

	/// Map a post object to container metadata (a post is a container-of-one). The
	/// @p media_referer, when set, is attached to every preview's fetch headers (site
	/// data — the mapping itself bakes no host).
	[[nodiscard]] virtual ImageContainerInfo post_to_container_info(
	    const boost::json::object& post,
	    std::optional<std::string_view> media_referer) const = 0;

	/// Map a post object to its media leaf; nullopt when it has no servable file. The
	/// @p media_referer, when set, is attached to the media (and poster) fetch headers.
	[[nodiscard]] virtual std::optional<ImageItem> post_to_item(
	    const boost::json::object& post,
	    std::optional<std::string_view> media_referer) const = 0;

	// --- suggest / autocomplete -------------------------------------------------

	/// An autocomplete request against @p base for @p partial, optionally narrowed to
	/// a @p kind axis.
	[[nodiscard]] virtual GetRequest suggest_request(std::string_view base,
	                                                 std::string partial,
	                                                 std::optional<std::string> kind) const = 0;

	/// Map an autocomplete response into suggestions.
	[[nodiscard]] virtual std::vector<SearchSuggestion> parse_suggestions(
	    const boost::json::value& envelope) const = 0;

	// --- url routing ------------------------------------------------------------

	/// The post id a container URL addresses, if any.
	[[nodiscard]] virtual std::optional<ImageContainerID> post_id_from_url(
	    const ParsedUrl& url) const = 0;

	/// The pool id a URL addresses, if any; nullopt for families without pools.
	[[nodiscard]] virtual std::optional<ImageContainerID> pool_id_from_url(
	    const ParsedUrl& /*url*/) const {
		return std::nullopt;
	}

	// --- pools (only meaningful when supports_pools()) --------------------------

	/// Pool-metadata fetch request against @p base.
	[[nodiscard]] virtual GetRequest pool_request(std::string_view /*base*/,
	                                              ImageContainerID /*id*/) const {
		return {};
	}

	/// Map a pool object to container metadata (a container-of-many).
	[[nodiscard]] virtual ImageContainerInfo pool_to_container_info(
	    const boost::json::object& /*pool*/) const {
		return {};
	}

	/// A request against @p base for one page of the pool's posts, in pool order.
	[[nodiscard]] virtual GetRequest pool_items_request(std::string_view /*base*/,
	                                                    ImageContainerID /*id*/,
	                                                    const GetFilters& /*filters*/) const {
		return {};
	}
};

} // namespace aniparse::engines
