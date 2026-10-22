/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/catalog/content/Catalog.hpp"

namespace aniparse {

CatalogRootGetter::CatalogRootGetter(CatalogScope scope) : scope_(std::move(scope)) {}

NetworkRequestTask<SearchCapabilities> CatalogRootGetter::search_support(RequestorContext context) const {
	co_return co_await search_support_for(std::move(context));
}

CatalogLatestSupport CatalogRootGetter::latest_support() const noexcept {
	return latest_support_for();
}

std::vector<SearchQueryError> CatalogRootGetter::validate_latest_filters(const GetFilters& filters) const {
	return validate_sort(latest_support().supported_sorts, filters.sort);
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

NetworkRequestTask<PageResults<CatalogEntry>> CatalogRootGetter::search(RequestorContext context, SearchRequestQuery query, GetFilters filters) const {
	co_return checked_page(co_await search_items(std::move(context), std::move(query), std::move(filters)));
}

NetworkRequestTask<PageResults<CatalogEntry>> CatalogRootGetter::latest(RequestorContext context, GetFilters filters) const {
	co_return checked_page(co_await latest_items(std::move(context), std::move(filters)));
}

NetworkRequestTask<std::vector<SearchSuggestion>> CatalogRootGetter::suggest(RequestorContext context, std::string partial, std::optional<std::string> kind) const {
	co_return co_await suggest_items(std::move(context), std::move(partial), std::move(kind));
}

NetworkRequestTask<CatalogEntry> CatalogRootGetter::parse_url(RequestorContext context, ParsedUrl url) const {
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

NetworkRequestTask<SearchCapabilities> CatalogRootGetter::search_support_for(RequestorContext) const { co_return SearchCapabilities{}; }
CatalogLatestSupport CatalogRootGetter::latest_support_for() const noexcept { return {}; }
NetworkRequestTask<PageResults<CatalogEntry>> CatalogRootGetter::search_items(RequestorContext, SearchRequestQuery, GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot search");
}
NetworkRequestTask<PageResults<CatalogEntry>> CatalogRootGetter::latest_items(RequestorContext, GetFilters) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot list latest items");
}
NetworkRequestTask<std::vector<SearchSuggestion>> CatalogRootGetter::suggest_items(RequestorContext, std::string, std::optional<std::string>) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot suggest tokens");
}
NetworkRequestTask<CatalogEntry> CatalogRootGetter::parse_entry_url(RequestorContext, ParsedUrl) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot open URLs");
}
NetworkRequestTask<CatalogEntry> CatalogRootGetter::restore_entry(SerializedGetterData, MediaKind) const {
	co_return make_response_error(RequestErrorCode::NotImplemented, "The catalog cannot restore entries");
}

} // namespace aniparse
