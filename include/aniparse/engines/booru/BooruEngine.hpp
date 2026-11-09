/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/net/Headers.hpp"
#include "aniparse/images/ImagesGetter.hpp"
#include "aniparse/net/Request.hpp"

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aniparse::engines {

/// Owned mapping of one decoded post, including an optional servable media leaf.
struct BooruPost {
	ImageContainerInfo info;
	std::optional<ImageItem> item;
};

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

	/**
	 * @brief Headers every request carries (a descriptive User-Agent, at minimum). A
	 * family-level default; the host is site data (@see BooruSite::api_hosts).
	 *
	 * Seeded onto the parser's config once (ParserProfile::configure), not stamped per request.
	 * @return The header set every request of a site of this family goes out with
	 */
	[[nodiscard]] virtual Headers api_headers() const = 0;

	/**
	 * @brief Whether this family exposes pools (an ordered container-of-many). Gates the
	 * pool branch of URL routing and the use of BooruPoolGetter. Default: no pools.
	 *
	 * When this is false the pool methods below are never called and need no override.
	 * @return True if the family has pools
	 */
	[[nodiscard]] virtual bool supports_pools() const noexcept { return false; }

	// --- search / listing -------------------------------------------------------

	/**
	 * @brief The sorts and suggestion capabilities this source advertises (no network).
	 *
	 * The table a query is validated against before a request is spent on it. Booru tags
	 * are open-vocabulary free text, so what is declared here is the sort keys the family
	 * can express as a metatag plus whether it offers autocomplete — not an enumerable
	 * filter set.
	 * @return The support table for the family's listing/search surface
	 */
	[[nodiscard]] virtual SearchCapabilities load_search_capabilities() const = 0;

	/**
	 * @brief A post-list request against @p base for @p tags with @p filters paging applied;
	 * a requested sort is folded into the tag string as this family's sort metatag.
	 *
	 * The one builder behind both search (tags given) and the newest-first feed (empty
	 * tags). A sort key the family cannot express is dropped rather than faked — which is
	 * why only the expressible ones are advertised in @ref load_search_capabilities.
	 * @param base The already-resolved fetch host to build the URL on
	 * @param tags The tag query, free text in the family's tag grammar; may be empty
	 * @param filters Paging window and requested sort
	 * @return The request to perform
	 */
	[[nodiscard]] virtual GetRequest list_request(std::string_view base,
	                                              std::string tags,
	                                              const GetFilters& filters) const = 0;

	/**
     * @brief Decode and map an entire list/search response once.
     * @param envelope The parsed response, borrowed for this call.
     * @param media_referer Optional site-specific Referer for media resources.
     * @return Owned posts, or UnexpectedResponse for malformed required data.
     */
	[[nodiscard]] virtual Result<std::vector<BooruPost>> parse_posts(
	    const boost::json::value& envelope,
	    std::optional<std::string_view> media_referer) const = 0;

	// --- single post (container-of-one) -----------------------------------------

	/**
	 * @brief A single-post fetch request against @p base, addressed by id.
	 * @param base The already-resolved fetch host to build the URL on
	 * @param id The post to fetch
	 * @return The request to perform
	 */
	[[nodiscard]] virtual GetRequest container_request(std::string_view base,
	                                                   ImageContainerID id) const = 0;

	/**
     * @brief Decode and map a single-post response once.
     * @param envelope The parsed response, borrowed for this call.
     * @param media_referer Optional site-specific Referer for media resources.
     * @return An owned post, NotFound for an absent post, or a conversion error.
     */
	[[nodiscard]] virtual Result<BooruPost> parse_post(
	    const boost::json::value& envelope,
	    std::optional<std::string_view> media_referer) const = 0;

	// --- suggest / autocomplete -------------------------------------------------

	/**
	 * @brief An autocomplete request against @p base for @p partial, optionally narrowed to
	 * a @p kind axis.
	 * @param base The already-resolved fetch host to build the URL on
	 * @param partial The partially typed tag to complete
	 * @param kind The axis to narrow to — one of the kinds declared in @ref load_search_capabilities;
	 *        nullopt (or a kind the family does not offer as its own query type) means
	 *        complete against all tags
	 * @return The request to perform
	 */
	[[nodiscard]] virtual GetRequest suggest_request(std::string_view base,
	                                                 std::string partial,
	                                                 std::optional<std::string> kind) const = 0;

	/**
	 * @brief Map an autocomplete response into suggestions.
	 *
	 * Each suggestion carries the token to search by, a label to show, and — where the
	 * family types its tags — the axis the tag belongs to.
	 * @param envelope The parsed autocomplete response
	 * @return The suggestions, or UnexpectedResponse for malformed data
	 */
	[[nodiscard]] virtual Result<std::vector<SearchSuggestion>> parse_suggestions(
	    const boost::json::value& envelope) const = 0;

	// --- url routing ------------------------------------------------------------

	/**
	 * @brief The post id a container URL addresses, if any.
	 *
	 * How a pasted URL becomes a getter, and how the parser decides a URL is one it can
	 * open at all. Families differ in where the id sits (a path segment, a query param),
	 * which is why the engine and not the parser answers this.
	 * @param url The URL to route, already parsed
	 * @return The post id, or nullopt when the URL addresses no post of this family
	 */
	[[nodiscard]] virtual std::optional<ImageContainerID> post_id_from_url(
	    const ParsedUrl& url) const = 0;

	/**
	 * @brief The pool id a URL addresses, if any; nullopt for families without pools.
	 *
	 * Takes the already-parsed URL to route.
	 * @return The pool id, or nullopt when the URL addresses no pool. Default: nullopt
	 */
	[[nodiscard]] virtual std::optional<ImageContainerID> pool_id_from_url(
	    const ParsedUrl& /*url*/) const {
		return std::nullopt;
	}

	// --- pools (only meaningful when supports_pools()) --------------------------

	/**
	 * @brief Pool-metadata fetch request against the given base.
	 *
	 * Takes the already-resolved fetch host to build the URL on, and the pool to fetch.
	 * @return The request to perform. Default: an empty request — never called unless
	 *         @ref supports_pools()
	 */
	[[nodiscard]] virtual GetRequest pool_request(std::string_view /*base*/,
	                                              ImageContainerID /*id*/) const {
		return {};
	}

	/**
	 * @brief Map a pool object to container metadata (a container-of-many).
	 *
	 * Takes the pool object of a pool-metadata response.
	 * @return The container metadata or a conversion error. Default: empty — never called unless
	 *         @ref supports_pools()
	 */
	[[nodiscard]] virtual Result<ImageContainerInfo> pool_to_container_info(
	    const boost::json::value& /*pool*/) const {
		return ImageContainerInfo{};
	}

	/**
	 * @brief A request for one page of a pool's posts, in pool order.
	 *
	 * Takes the already-resolved fetch host, the pool whose posts are wanted, and the
	 * paging window. A pool is paged like any listing rather than materialized whole, so
	 * its items can be streamed; the response is decoded with @ref parse_posts, like a listing's.
	 * @return The request to perform. Default: an empty request — never called unless
	 *         @ref supports_pools()
	 */
	[[nodiscard]] virtual GetRequest pool_items_request(std::string_view /*base*/,
	                                                    ImageContainerID /*id*/,
	                                                    const GetFilters& /*filters*/) const {
		return {};
	}

protected:
	BooruEngine() = default;
	BooruEngine(const BooruEngine&) = default;
	BooruEngine(BooruEngine&&) = default;
	BooruEngine& operator=(const BooruEngine&) = default;
	BooruEngine& operator=(BooruEngine&&) = default;
};

} // namespace aniparse::engines
