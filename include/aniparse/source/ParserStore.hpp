/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/detail/DomainStore.hpp"
#include "aniparse/source/Parser.hpp"
#include "aniparse/net/ParsedUrl.hpp"

#include <string>
#include <map>
#include <optional>
#include <vector>

namespace aniparse {

/**
 * @brief A routed URL with its provider, selected catalog.
 *
 * Use catalog_id to select a unified catalog root. Catalog selection is independent of media kind. Sources with an image-container
 * API may accept URLs through valid_for_url() without selecting a catalog.
 */
struct UrlRoute {
	std::shared_ptr<Parser> parser; ///< Provider that owns the URL.
	ParsedUrl                 url; ///< Parsed URL passed to the selected root.
	std::optional<std::string> catalog_id; ///< Selected catalog ID; absent for image-container routes.
};

/**
 * @brief Class for storing and retrieving parsers
 */
class ParserStore {
public:
	struct ParserDomainEmitter {
		void operator()(const Parser& parser, EmplaceDomainsSink& context) const {
			parser.emplace_domains(context);
		}
	};
	using Domains = detail::DomainStore<Parser, EmplaceDomainsSink, ParserDomainEmitter>;
	using DomainEdit = Domains::Edit;

	class Edit {
	public:
		Edit(const Edit&) = delete;
		Edit& operator=(const Edit&) = delete;
		Edit(Edit&&) noexcept = default;
		Edit& operator=(Edit&&) noexcept = default;

		std::shared_ptr<Parser> add_parser(std::shared_ptr<Parser> parser);
		bool remove_parser(std::string_view identifier);
		void refresh_domains(std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains);
		void commit();

	private:
		friend class ParserStore;
		explicit Edit(DomainEdit edit) : edit_(std::move(edit)) {}
		DomainEdit edit_;
	};

	ParserStore() = default;

	/// Start a local mutable draft; commit() publishes one new routing snapshot.
	[[nodiscard]] Edit begin_edit() ANIPARSE_LIFETIMEBOUND { return Edit(domains_.begin_edit()); }

	/**
	 * @brief Add parser to the store
	 * @see Parser
	 * Calls parser->emplace_domains(...), when the parser have to add its domains.
	 * Later the parser can be got from @ref find_by_domain, @ref find_for_url, @ref find_by_key
	 * @param parser The parser to add
	 * @throw std::logic_error If the parser identifier is conflicting with existing parsers
	 * @return Stored parser
	 */
	std::shared_ptr<Parser> add_parser(std::shared_ptr<Parser> parser);

	/**
	 * @brief Search parser suitable for the domain
	 * @see Parser
	 * @param domain The domain to search parser for
	 * @return Parser if found, empty pointer otherwise
	 */
	std::shared_ptr<Parser> find_by_domain(std::string_view domain);

	/**
	 * @brief Search parser suitable for the url
	 * @see Parser
	 * @param url The url to search parser for
	 * @return Parser if found, empty pointer otherwise
	 */
	std::shared_ptr<Parser> find_for_url(std::string_view url);

	/**
	 * @brief Search parser by it's unique identifier
	 * @see Parser
	 * @param key Unique parser identifier
	 * @return Parser if found, empty pointer otherwise
	 */
	std::shared_ptr<Parser> find_by_key(std::string_view key) const;

	/**
	 * @brief All registered parsers, in identifier order.
	 * A snapshot copy for enumeration — e.g. a UI listing the available sources,
	 * each parser exposing info()/identifier()/capabilities(). Not a live view:
	 * parsers added afterwards are not reflected until the next call.
	 * @return The registered parsers.
	 */
	[[nodiscard]] std::vector<std::shared_ptr<Parser>> parsers() const;

	/**
	 * @brief Route a URL to its provider and catalog in one step.
	 * Parses the URL once, finds the owning parser (domain match + valid_for_url),
	 * and asks it which catalog the URL selects (suggest_catalog). The caller
	 * then selects make_catalog_getter(*route.catalog_id) and calls parse_url(route.url).
	 * An image-container route without catalog_id uses make_images_getter().
	 *
	 * Routing is local: the URL need not use the provider's current canonical domain
	 * or resolve over the network. A registered legacy domain works while the parser
	 * still recognizes the URL format. The receiving getter interprets its identity;
	 * subsequent requests use the destination's own profile and endpoint settings.
	 * The caller must not reuse the originating parser's profile or manufacture a
	 * serialized handle for the destination parser. A canonical domain alone cannot
	 * compensate for an unsupported URL format.
	 * @param url The URL to route
	 * @return The route, or nullopt if the URL is unparsable or no parser handles it
	 */
	std::optional<UrlRoute> route_url(std::string_view url);

	/**
	 * @brief Replace the volatile (catalog-supplied) domains and rebuild routing.
	 * Each parser's static domains (from emplace_domains) are always kept; the
	 * volatile domains here are merged on top, keyed by parser identifier. Passing
	 * an empty map falls back to static domains only. The routing index is rebuilt
	 * as a fresh snapshot and swapped in atomically, so lookups already in progress
	 * keep using their snapshot until they finish.
	 * @param volatile_domains Parser identifier -> extra domains it should route
	 */
	void refresh_domains(std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains);

private:
	Domains domains_;
};
} // namespace aniparse
