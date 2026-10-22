/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/catalog/CatalogRootGetter.hpp"

namespace aniparse {

CatalogRootGetter::CatalogRootGetter(CatalogInfo catalog) : info_(std::move(catalog)) {}

RequestTask<SearchCapabilities> CatalogRootGetter::load_search_capabilities(RequestContext context) const {
	co_return co_await load_search_capabilities_for(std::move(context));
}

CatalogLatestCapabilities CatalogRootGetter::latest_capabilities() const noexcept {
	return latest_capabilities_for();
}

std::vector<SearchQueryError> CatalogRootGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_capabilities().supported_sorts, filters.sort);
}

namespace {
Result<PageResults<CatalogEntry>> checked_page(Result<PageResults<CatalogEntry>> page) {
	if (page) {
		for (const auto& row : page->results) {
			if (row.item.empty()) return make_request_error(RequestErrorCode::UnexpectedResponse, "Catalog card has no content getter");
		}
	}
	return page;
}
} // namespace

RequestTask<PageResults<CatalogEntry>> CatalogRootGetter::search(RequestContext context, SearchRequestQuery query, GetFilters filters) const {
	co_return checked_page(co_await search_items(std::move(context), std::move(query), std::move(filters)));
}

RequestTask<PageResults<CatalogEntry>> CatalogRootGetter::latest(RequestContext context, GetFilters filters) const {
	co_return checked_page(co_await latest_items(std::move(context), std::move(filters)));
}

RequestTask<std::vector<SearchSuggestion>> CatalogRootGetter::suggest(RequestContext context, std::string partial, std::optional<std::string> kind) const {
	co_return co_await suggest_items(std::move(context), std::move(partial), std::move(kind));
}

RequestTask<CatalogEntry> CatalogRootGetter::parse_url(RequestContext context, ParsedUrl url) const {
	auto entry = co_await parse_entry_url(std::move(context), std::move(url));
	if (entry && entry->empty()) co_return make_request_error(RequestErrorCode::UnexpectedResponse, "Catalog card has no content getter");
	co_return entry;
}

RequestTask<CatalogEntry> CatalogRootGetter::from_serialized(SerializedGetterData data, MediaKind kind) const {
	if (kind != MediaKind::Manga && kind != MediaKind::Anime) {
		co_return make_request_error(RequestErrorCode::InvalidArguments, "Unsupported serialized media kind");
	}
	auto entry = co_await restore_entry(std::move(data), kind);
	if (entry && !entry->has(kind)) co_return make_request_error(RequestErrorCode::UnexpectedResponse, "Restored card has no requested getter");
	co_return entry;
}

RequestTask<SearchCapabilities> CatalogRootGetter::load_search_capabilities_for(RequestContext) const { co_return SearchCapabilities{}; }
CatalogLatestCapabilities CatalogRootGetter::latest_capabilities_for() const noexcept { return {}; }
RequestTask<PageResults<CatalogEntry>> CatalogRootGetter::search_items(RequestContext, SearchRequestQuery, GetFilters) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The catalog cannot search");
}
RequestTask<PageResults<CatalogEntry>> CatalogRootGetter::latest_items(RequestContext, GetFilters) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The catalog cannot list latest items");
}
RequestTask<std::vector<SearchSuggestion>> CatalogRootGetter::suggest_items(RequestContext, std::string, std::optional<std::string>) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The catalog cannot suggest tokens");
}
RequestTask<CatalogEntry> CatalogRootGetter::parse_entry_url(RequestContext, ParsedUrl) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The catalog cannot open URLs");
}
RequestTask<CatalogEntry> CatalogRootGetter::restore_entry(SerializedGetterData, MediaKind) const {
	co_return make_request_error(RequestErrorCode::NotImplemented, "The catalog cannot restore entries");
}

} // namespace aniparse
