/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/images/ImagesGetter.hpp"

namespace aniparse::engines {

struct BooruSite;

/**
 * @brief Root images getter for a booru source: search by tags, autocomplete, latest,
 * and URL routing over a post index. Generic over the site — the fetch host comes from
 * the injected @ref BooruSite and every request/mapping from its engine (both outlive
 * the getter, static instances) — so one implementation serves every booru family.
 * Stateless: one instance serves every request.
 *
 * The free-text query is the raw tag string; a supported sort is folded into the
 * engine's sort metatag. Search results and single-URL lookups both yield generic
 * container getters bound to the same site.
 */
class BooruImagesGetter : public ImagesGetter {
public:
	explicit BooruImagesGetter(const BooruSite& site) : site_(site) {}

	NetworkRequestTask<SearchCapabilities> load_search_capabilities(RequestContext context) const override;

	NetworkRequestTask<std::vector<SearchSuggestion>> suggest(
	    RequestContext context,
	    std::string partial,
	    std::optional<std::string> kind) const override;

	NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> search(
	    RequestContext context,
	    SearchRequestQuery query,
	    GetFilters filters) const override;

	NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> latest(
	    RequestContext context,
	    GetFilters filters) const override;

	NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> parse_url(
	    RequestContext context,
	    ParsedUrl url) const override;

	NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> from_serialized(
	    SerializedGetterData data) const override;

protected:
	const BooruSite& site_;
};

} // namespace aniparse::engines
