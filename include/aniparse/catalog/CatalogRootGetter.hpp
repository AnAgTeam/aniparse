/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/catalog/CatalogEntry.hpp"
#include "aniparse/catalog/CatalogInfo.hpp"
#include "aniparse/net/ParsedUrl.hpp"
#include "aniparse/catalog/Search.hpp"

namespace aniparse {

/**
 * @brief Sorts and flags accepted by the selected catalog's latest operation.
 */
struct CatalogLatestCapabilities {
	SupportedSorts supported_sorts; ///< Accepted sort keys and directions.
	CapabilitiesFlags capabilities; ///< Listing capability flags.
};

/**
 * @brief One catalog interface bound to a parser-selected catalog at construction.
 *
 * Select the catalog once through Parser::make_catalog_getter. Search, latest, filter
 * discovery, suggestions and opening then use that same catalog. The catalog's
 * kinds and capabilities are metadata; they do not gate calls or filter results.
 * Successful operations return nonempty cards with native pagination unchanged.
 */
class CatalogRootGetter {
public:
	virtual ~CatalogRootGetter() = default;

	/**
	 * @brief Bind this root to a concrete catalog.
	 * @param catalog Owned copy of the parser's selected catalog descriptor.
	 *              The default creates a standalone root with unknown metadata.
	 */
	explicit CatalogRootGetter(CatalogInfo catalog = { .id = "main" });

	/**
	 * @brief Read the selected catalog without I/O.
	 * @return Descriptor owned by this root, valid for its lifetime.
	 */
	[[nodiscard]] const CatalogInfo& info() const noexcept { return info_; }

	/**
	 * @brief Fetch the selected catalog's filter declaration.
	 * @param context Request context.
	 * @return Filter support, or a request error.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<SearchCapabilities> load_search_capabilities(RequestContext context) const;

	/**
	 * @brief Read ordering support for the selected catalog's latest operation.
	 * @return Accepted sorts and listing flags.
	 */
	CatalogLatestCapabilities latest_capabilities() const noexcept;

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
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<PageResults<CatalogEntry>> search(RequestContext context, SearchRequestQuery query, GetFilters filters) const;

	/**
	 * @brief Fetch one recent-items page in the selected catalog.
	 * @param context Request context.
	 * @param filters Pagination and ordering.
	 * @return Nonempty cards with native offsets, or an error.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<PageResults<CatalogEntry>> latest(RequestContext context, GetFilters filters) const;

	/**
	 * @brief Complete a search token in the selected catalog.
	 * @param context Request context.
	 * @param partial Partial token.
	 * @param kind Optional search-filter axis, not the catalog's media kinds.
	 * @return Suggestions, or an error.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<std::vector<SearchSuggestion>> suggest(RequestContext context, std::string partial, std::optional<std::string> kind = std::nullopt) const;

	/**
	 * @brief Open a URL belonging to the selected catalog.
	 * @param context Request context.
	 * @param url Source URL.
	 * @return A nonempty card, or an error.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<CatalogEntry> parse_url(RequestContext context, ParsedUrl url) const;

	/**
	 * @brief Restore an item's durable identity within the selected catalog.
	 * @param data Unchanged parser-owned blob emitted by an item getter.
	 * @param kind Format stored alongside the catalog ID and blob.
	 * @return A card containing the requested format, or an error.
	 * @note Equal IDs in different formats or catalogs remain independent.
	 * Normal restoration decodes identity locally without network requests or
	 * populating preview data. Resolving a legacy identity may suspend when the
	 * parser must migrate an older format.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<CatalogEntry> from_serialized(SerializedGetterData data, MediaKind kind) const;

protected:
	/**
	 * @brief Implement filter discovery for the bound catalog.
	 * @param context Request context.
	 * @return Support, or an error; empty by default.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<SearchCapabilities> load_search_capabilities_for(RequestContext context) const;

	/**
	 * @brief Implement latest declarations for the bound catalog.
	 * @return Accepted sorts and flags; empty by default.
	 */
	virtual CatalogLatestCapabilities latest_capabilities_for() const noexcept;

	/**
	 * @brief Implement native search in the bound catalog.
	 * @param context Request context.
	 * @param query Search values.
	 * @param filters Pagination and ordering.
	 * @return A page, or an error; NotImplemented by default.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<PageResults<CatalogEntry>> search_items(RequestContext context, SearchRequestQuery query, GetFilters filters) const;

	/**
	 * @brief Implement native latest in the bound catalog.
	 * @param context Request context.
	 * @param filters Pagination and ordering.
	 * @return A page, or an error; NotImplemented by default.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<PageResults<CatalogEntry>> latest_items(RequestContext context, GetFilters filters) const;

	/**
	 * @brief Implement autocomplete in the bound catalog.
	 * @param context Request context.
	 * @param partial Partial token.
	 * @param kind Optional search-filter axis.
	 * @return Suggestions, or an error; NotImplemented by default.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<std::vector<SearchSuggestion>> suggest_items(RequestContext context, std::string partial, std::optional<std::string> kind) const;

	/**
	 * @brief Implement URL opening in the bound catalog.
	 * @param context Request context.
	 * @param url Source URL.
	 * @return A card, or an error; NotImplemented by default.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<CatalogEntry> parse_entry_url(RequestContext context, ParsedUrl url) const;

	/**
	 * @brief Implement identity restoration in the bound catalog.
	 * @param data Parser-owned identity.
	 * @param kind Requested format.
	 * @return A card, or an error; NotImplemented by default.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<CatalogEntry> restore_entry(SerializedGetterData data, MediaKind kind) const;

protected:
	CatalogRootGetter(const CatalogRootGetter&) = default;
	CatalogRootGetter(CatalogRootGetter&&) = default;
	CatalogRootGetter& operator=(const CatalogRootGetter&) = default;
	CatalogRootGetter& operator=(CatalogRootGetter&&) = default;

private:
	CatalogInfo info_;
};

} // namespace aniparse
