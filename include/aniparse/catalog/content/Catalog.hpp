/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/catalog/content/Entry.hpp"
#include "aniparse/catalog/content/Scope.hpp"
#include "aniparse/types/ParsedUrl.hpp"
#include "aniparse/types/Search.hpp"

namespace aniparse {

/**
 * @brief Sorts and flags accepted by the selected catalog's latest operation.
 */
struct CatalogLatestSupport {
	SupportedSorts supported_sorts; ///< Accepted sort keys and directions.
	CompatibilitiesFlags compatibilities; ///< Listing capability flags.
};

/**
 * @brief One catalog interface bound to a parser-selected scope at construction.
 *
 * Select the scope once through Parser::catalog_getter. Search, latest, filter
 * discovery, suggestions and opening then use that same catalog. The scope's
 * kinds and capabilities are metadata; they do not gate calls or filter results.
 * Successful operations return nonempty cards with native pagination unchanged.
 */
class CatalogRootGetter {
public:
	virtual ~CatalogRootGetter() = default;

	/**
	 * @brief Bind this root to a concrete catalog.
	 * @param scope Owned copy of the parser's selected scope descriptor.
	 *              The default creates a standalone root with unknown metadata.
	 */
	explicit CatalogRootGetter(CatalogScope scope = { .id = "main" });

	/**
	 * @brief Read the selected scope without I/O.
	 * @return Descriptor owned by this root, valid for its lifetime.
	 */
	[[nodiscard]] const CatalogScope& scope() const noexcept { return scope_; }

	/**
	 * @brief Fetch the selected catalog's filter declaration.
	 * @param context Request context.
	 * @return Filter support, or a request error.
	 */
	NetworkRequestTask<SearchCapabilities> search_support(RequestorContext context) const;

	/**
	 * @brief Read ordering support for the selected catalog's latest operation.
	 * @return Accepted sorts and listing flags.
	 */
	CatalogLatestSupport latest_support() const noexcept;

	/**
	 * @brief Validate latest ordering against this catalog's declaration.
	 * @param filters Requested ordering and pagination.
	 * @return Sort violations, empty when valid.
	 */
	std::vector<SearchQueryError> validate_latest_filters(const GetFilters& filters) const;

	/**
	 * @brief Search one page in the selected catalog.
	 * @param context Request context.
	 * @param query Text and structured filters.
	 * @param filters Pagination and ordering.
	 * @return Nonempty cards with native offsets, or an error.
	 */
	NetworkRequestTask<PageResults<CatalogEntry>> search(RequestorContext context, SearchRequestQuery query, GetFilters filters) const;

	/**
	 * @brief Fetch one recent-items page in the selected catalog.
	 * @param context Request context.
	 * @param filters Pagination and ordering.
	 * @return Nonempty cards with native offsets, or an error.
	 */
	NetworkRequestTask<PageResults<CatalogEntry>> latest(RequestorContext context, GetFilters filters) const;

	/**
	 * @brief Complete a search token in the selected catalog.
	 * @param context Request context.
	 * @param partial Partial token.
	 * @param kind Optional search-filter axis, not the catalog's media kinds.
	 * @return Suggestions, or an error.
	 */
	NetworkRequestTask<std::vector<SearchSuggestion>> suggest(RequestorContext context, std::string partial, std::optional<std::string> kind = std::nullopt) const;

	/**
	 * @brief Open a URL belonging to the selected catalog.
	 * @param context Request context.
	 * @param url Source URL.
	 * @return A nonempty card, or an error.
	 */
	NetworkRequestTask<CatalogEntry> parse_url(RequestorContext context, ParsedUrl url) const;

	/**
	 * @brief Restore an item's durable identity within the selected catalog.
	 * @param data Unchanged parser-owned blob emitted by an item getter.
	 * @param kind Format stored alongside the scope ID and blob.
	 * @return A card containing the requested format, or an error.
	 * @note Equal numeric IDs in different formats or scopes remain independent.
	 * Normal restoration decodes identity locally without network requests or
	 * populating preview data. Resolving a legacy identity may suspend when the
	 * parser must migrate an older format.
	 */
	NetworkRequestTask<CatalogEntry> from_serialized(SerializedGetterData data, MediaKind kind) const;

protected:
	/**
	 * @brief Implement filter discovery for the bound scope.
	 * @param context Request context.
	 * @return Support, or an error; empty by default.
	 */
	virtual NetworkRequestTask<SearchCapabilities> search_support_for(RequestorContext context) const;

	/**
	 * @brief Implement latest declarations for the bound scope.
	 * @return Accepted sorts and flags; empty by default.
	 */
	virtual CatalogLatestSupport latest_support_for() const noexcept;

	/**
	 * @brief Implement native search in the bound scope.
	 * @param context Request context.
	 * @param query Search values.
	 * @param filters Pagination and ordering.
	 * @return A page, or an error; NotImplemented by default.
	 */
	virtual NetworkRequestTask<PageResults<CatalogEntry>> search_items(RequestorContext context, SearchRequestQuery query, GetFilters filters) const;

	/**
	 * @brief Implement native latest in the bound scope.
	 * @param context Request context.
	 * @param filters Pagination and ordering.
	 * @return A page, or an error; NotImplemented by default.
	 */
	virtual NetworkRequestTask<PageResults<CatalogEntry>> latest_items(RequestorContext context, GetFilters filters) const;

	/**
	 * @brief Implement autocomplete in the bound scope.
	 * @param context Request context.
	 * @param partial Partial token.
	 * @param kind Optional search-filter axis.
	 * @return Suggestions, or an error; NotImplemented by default.
	 */
	virtual NetworkRequestTask<std::vector<SearchSuggestion>> suggest_items(RequestorContext context, std::string partial, std::optional<std::string> kind) const;

	/**
	 * @brief Implement URL opening in the bound scope.
	 * @param context Request context.
	 * @param url Source URL.
	 * @return A card, or an error; NotImplemented by default.
	 */
	virtual NetworkRequestTask<CatalogEntry> parse_entry_url(RequestorContext context, ParsedUrl url) const;

	/**
	 * @brief Implement identity restoration in the bound scope.
	 * @param data Parser-owned identity.
	 * @param kind Requested format.
	 * @return A card, or an error; NotImplemented by default.
	 */
	virtual NetworkRequestTask<CatalogEntry> restore_entry(SerializedGetterData data, MediaKind kind) const;

private:
	CatalogScope scope_;
};

} // namespace aniparse
