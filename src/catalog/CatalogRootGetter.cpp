/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/catalog/CatalogRootGetter.hpp"

namespace aniparse {

CatalogRootGetter::CatalogRootGetter(CatalogInfo scope) : scope_(std::move(scope)) {}

NetworkRequestTask<SearchCapabilities> CatalogRootGetter::load_search_capabilities(RequestContext context) const {
	co_return co_await load_search_capabilities_for(std::move(context));
}

CatalogLatestCapabilities CatalogRootGetter::latest_capabilities() const noexcept {
	return latest_capabilities_for();
}

std::vector<SearchQueryError> CatalogRootGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_capabilities().supported_sorts, filters.sort);
}

namespace {
Response<PageResults<CatalogEntry>> checked_page(Response<PageResults<CatalogEntry>> page) {
	if (page) {
		for (const auto& row : page->results) {
			if (row.item.empty()) return make_response_error(RequestErrorCode::UnexpectedResponse, "Catalog card has no content getter");
		}
	}
	return page;
}
} // namespace

NetworkRequestTask<PageResults<CatalogEntry>> CatalogRootGetter::search(RequestContext context, SearchRequestQuery query, GetFilters filters) const {
	co_return checked_page(co_await search_items(std::move(context), std::move(query), std::move(filters)));
}

NetworkRequestTask<PageResults<CatalogEntry>> CatalogRootGetter::latest(RequestContext context, GetFilters filters) const {
	co_return checked_page(co_await latest_items(std::move(context), std::move(filters)));
}

NetworkRequestTask<std::vector<SearchSuggestion>> CatalogRootGetter::suggest(RequestContext context, std::string partial, std::optional<std::string> kind) const {
	co_return co_await suggest_items(std::move(context), std::move(partial), std::move(kind));
}

NetworkRequestTask<CatalogEntry> CatalogRootGetter::parse_url(RequestContext context, ParsedUrl url) const {
	auto entry = co_await parse_entry_url(std::move(context), std::move(url));
	if (entry && entry->empty()) co_return make_response_error(RequestErrorCode::UnexpectedResponse, "Catalog card has no content getter");
	co_return entry;
}

NetworkRequestTask<CatalogEntry> CatalogRootGetter::from_serialized(SerializedGetterData data, MediaKind kind) const {
	if (kind != MediaKind::Manga && kind != MediaKind::Anime) {
		co_return make_response_error(RequestErrorCode::InvalidArguments, "Unsupported serialized media kind");
	}
	auto entry = co_await restore_entry(std::move(data), kind);
	if (entry && !entry->has(kind)) co_return make_response_error(RequestErrorCode::UnexpectedResponse, "Restored card has no requested getter");
	co_return entry;
}

NetworkRequestTask<SearchCapabilities> CatalogRootGetter::load_search_capabilities_for(RequestContext) const { co_return SearchCapabilities{}; }
CatalogLatestCapabilities CatalogRootGetter::latest_capabilities_for() const noexcept { return {}; }
NetworkRequestTask<PageResults<CatalogEntry>> CatalogRootGetter::search_items(RequestContext, SearchRequestQuery, GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot search");
}
NetworkRequestTask<PageResults<CatalogEntry>> CatalogRootGetter::latest_items(RequestContext, GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot list latest items");
}
NetworkRequestTask<std::vector<SearchSuggestion>> CatalogRootGetter::suggest_items(RequestContext, std::string, std::optional<std::string>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot suggest tokens");
}
NetworkRequestTask<CatalogEntry> CatalogRootGetter::parse_entry_url(RequestContext, ParsedUrl) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot open URLs");
}
NetworkRequestTask<CatalogEntry> CatalogRootGetter::restore_entry(SerializedGetterData, MediaKind) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot restore entries");
}

} // namespace aniparse
