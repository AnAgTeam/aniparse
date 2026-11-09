/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/FlagsBitfield.hpp"
#include "aniparse/source/Capabilities.hpp"
#include "aniparse/catalog/ParseResult.hpp"
#include "aniparse/source/ParserInfo.hpp"
#include "aniparse/catalog/CatalogInfo.hpp"
#include "aniparse/net/ParsedUrl.hpp"
#include "aniparse/source/ParserProfile.hpp"
#include "aniparse/images/ImagesGetter.hpp"
#include "aniparse/manga/MangaGetter.hpp"

#include "aniparse/net/CancellingTask.hpp"
#include <memory>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <optional>
#include <vector>

namespace aniparse {
struct ImagesGetter;
class CatalogRootGetter;
class OverridesSink;
class RequestContext;

/**
 * @brief The sink a parser declares its domains into.
 *
 * A collecting seam, not a container the parser owns: @ref Parser::emplace_domains
 * is handed one and calls @ref add_domain once per host it answers for, without
 * knowing what the caller does with them (ParserStore builds its URL-routing index
 * out of them; another consumer could just list them). The reference is valid only
 * for the duration of the emplace_domains call — a parser must not retain it.
 */
struct EmplaceDomainsSink {
	virtual ~EmplaceDomainsSink() = default;

	/**
	 * @brief Declare one host the parser answers for.
	 *
	 * A bare registrable host, without scheme or path (e.g. "example.com").
	 * Subdomains of it match too, so a source needs one entry per registrable
	 * domain, not per host; a source with several domains (an alternate TLD, a
	 * short link domain) calls this once for each.
	 * @param domain The domain to declare; borrowed, copied by the sink if kept.
	 */
	virtual void add_domain(std::string_view domain) = 0;

protected:
	EmplaceDomainsSink() = default;
	EmplaceDomainsSink(const EmplaceDomainsSink&) = default;
	EmplaceDomainsSink(EmplaceDomainsSink&&) = default;
	EmplaceDomainsSink& operator=(const EmplaceDomainsSink&) = default;
	EmplaceDomainsSink& operator=(EmplaceDomainsSink&&) = default;
};

/**
 * @brief One content source: the object a consumer starts from, and the object a
 * new source is written as.
 *
 * A parser is the declarative half of a source. It does no fetching itself —
 * instead it states what the source *is* and hands out the objects that do the
 * work:
 *
 * - **Identity and presentation** — @ref identifier (the stable key everything
 *   else is keyed by: registration, config, remote overrides) and @ref info
 *   (name and artwork for a source list).
 * - **Reach** — @ref emplace_domains declares the hosts the source answers for,
 *   and @ref suggest_catalog classifies a URL under them, which together let
 *   ParserStore route an arbitrary URL to this parser and to the right kind of
 *   getter. ParserProfile::mirrors declares the base URLs its requests actually go to.
 * - **Catalogs** — @ref catalogs lists stable catalog IDs, possible formats and
 *   per-catalog UI metadata. @ref make_catalog_getter binds a common root to one ID.
 *   @ref capabilities supplies an aggregate summary for legacy consumers.
 * - **Configuration** — @ref profiles declares independent configuration and login
 *   profiles. Each catalog selects its profile by ID; user state lives outside
 *   the parser in ParserConfig / RequestContext.
 * - **Getters** — @ref make_catalog_getter selects a manga/anime catalog;
 *   @ref make_images_getter retains the separate image-container API —
 *   the root objects that search, list and fetch. A parser implements only the
 *   categories its source has; the rest stay null.
 *
 * A parser instance is registered once (ParserStore holds it by shared_ptr) and
 * used from anywhere afterwards: every method is const and must stay so in an
 * override. A parser therefore holds no mutable state — the per-run state lives in
 * the ParserConfig / RequestContext the caller passes to a getter, which is what
 * makes one parser instance safe to use concurrently and safe to reconfigure
 * (different mirror, different login) without cloning it.
 *
 * ### Implementing a source
 *
 * Derive and implement the three pure virtuals — @ref info, @ref identifier,
 * @ref emplace_domains — then override the getter factory
 * for each category the source offers, returning a root getter that performs the
 * requests. Everything else has a working default: URL entry is opt-in
 * (@ref suggest_catalog), as are per-request defaults (ParserProfile::configure), mirrors
 * (ParserProfile::mirrors) and login (ParserProfile::authenticate). Registering the finished
 * parser with a ParserStore is what makes it routable.
 */
struct Parser {
	virtual ~Parser() = default;

	/**
	 * @see ParserInfo
	 * @return Display metadata (name, language, artwork) for source listings.
	 */
	virtual ParserInfo info() const = 0;

	/** 
	 * @return Unique identifier for the parser
	 */
	virtual std::string identifier() const = 0;

	/**
	 * @brief Check whether the parser can handle the given URL.
	 * A coarse per-parser gate ("is this URL mine?"), consulted during URL
	 * routing after the domain matches. By default it is derived from
	 * @ref suggest_catalog (true when a catalog is selected), so a parser with
	 * URL entry only overrides suggest_catalog. Override this for exotic gating.
	 * @param url Parsed URL to check
	 * @return true if the parser handles the URL, false otherwise
	 */
	virtual bool valid_for_url(const ParsedUrl& url) const;

	/**
	 * @brief Select the catalog for a URL.
	 * The parser knows its own path/query scheme, so it classifies the URL to
	 * route parse_url to the right getter. Classify on whatever the URL carries —
	 * path, query (e.g. YouTube's ?v=), or host. The default is an empty suggestion.
	 * @param url Parsed URL
	 * @return An owned catalog ID, or std::nullopt when the URL does not select a catalog.
	 */
	virtual std::optional<std::string> suggest_catalog(const ParsedUrl& url) const;

	/**
	 * @brief Read provider-wide summary metadata for legacy source-list consumers.
	 * @return The union of catalog flags and legacy store-kind bits by default.
	 * @note New consumers inspect catalogs() for catalog-specific kinds and
	 *       capabilities. Reading and watching are catalog-local metadata fields.
	 */
	virtual ParserCapabilities capabilities() const;

	/**
	 * @see EmplaceDomainsSink
	 * Add parser domains to the sink
	 * This function is called whenever someone wants
	 * to add the parser domains to store
	 * @param sink Sink that receives the declared domains
	 */
	virtual void emplace_domains(EmplaceDomainsSink& sink) const = 0;

	/**
	 * @brief Declare the parser's overridable defaults into @p sink.
	 *
	 * The remote overrides' authoring data comes from LIVE objects, not from
	 * parsed source text: the dumper hands each registered parser an OverridesSink
	 * and the parser declares what a hotfix may override — its selector sets
	 * (`sink.emplace_set<...Selectors>()`, which strictly compiles every
	 * built-in literal through the real engine); request origins are
	 * declared separately through ParserProfile.
	 *
	 * Contract: an override DECLARES only — it calls sink methods and never
	 * stores the sink (valid only for the duration of the call). The empty
	 * default means "no override sets": such a parser simply
	 * has no selector hotfix channel, discovered at first use.
	 * @param sink The sink collecting this parser's override defaults
	 * @throws std::logic_error On a cross-set key collision or a broken built-in
	 * literal; these programming errors must fail overrides generation.
	 */
	virtual void emplace_overrides([[maybe_unused]] OverridesSink& sink) const {}

	/**
	 * @brief Enumerate independently configured request profiles.
	 * @return Stable, nonempty, unique profile IDs and display names.
	 * @note The default is one anonymous profile named main.
	 * @throws std::logic_error If the list is empty or IDs are empty or duplicated.
	 */
	std::vector<ProfileInfo> profiles() const;

	/**
	 * @brief Create an independently owned profile without performing network requests.
	 * @param profile_id Stable ID obtained from profiles() or a catalog.
	 * @return A fresh profile owned by the caller, or nullptr for an unknown ID.
	 * @note The caller decides how long to retain or share the profile; the parser
	 * does not cache instances or impose singleton lifetime.
	 * @throws std::logic_error On invalid descriptors or a mismatched factory result.
	 */
	std::unique_ptr<ParserProfile> make_profile(std::string_view profile_id) const;

	/**
	 * @brief The root of the source's image library: image containers (posts,
	 * pools, galleries), their search and their listings.
	 *
	 * A fresh getter, owned by the caller; getters are cheap and stateless, so one
	 * may be made per use rather than cached. It carries no config — the caller
	 * passes a RequestContext (captured through ProfileSession::snapshot) into each call.
	 * Default: nullptr.
	 * @return The root images getter, or nullptr if the source has no image library.
	 */
	virtual std::unique_ptr<ImagesGetter> make_images_getter() const;

	/**
	 * @brief Enumerate this provider's independently selectable catalogues.
	 * @return Catalog descriptions with stable, nonempty, provider-local IDs.
	 * @note Multiple catalogs can advertise the same format. This is metadata only;
	 *       enumeration performs no requests. Sources without catalogs return an empty list.
	 * @throws std::logic_error If IDs are empty or duplicated, profile descriptors
	 * are invalid, or a catalog references an undeclared profile.
	 */
	std::vector<CatalogInfo> catalogs() const;

	/**
	 * @brief Create a common root bound to one explicitly selected catalog.
	 * @param catalog_id Opaque ID obtained from catalogs(), copied before returning.
	 * @return A fresh root, or nullptr for an unknown catalog.
	 * @throws std::logic_error On invalid descriptors, a null factory result, or
	 * a root bound to another catalog or request profile.
	 * @note No format or capability metadata is used to disable operations.
	 */
	std::unique_ptr<CatalogRootGetter> make_catalog_getter(std::string_view catalog_id) const;

protected:
	Parser() = default;
	Parser(const Parser&) = default;
	Parser(Parser&&) = default;
	Parser& operator=(const Parser&) = default;
	Parser& operator=(Parser&&) = default;

	/**
	 * @brief Declare configuration and authentication profiles without I/O.
	 * @return A nonempty list of descriptors; defaults to the main profile.
	 */
	virtual std::vector<ProfileInfo> profile_descriptors() const;

	/**
	 * @brief Declare independently selectable catalogs without I/O.
	 * @return Descriptors referencing declared profiles; empty by default.
	 */
	virtual std::vector<CatalogInfo> catalog_descriptors() const;

	/**
	 * @brief Construct the behavior of a declared profile.
	 * @param info Owned descriptor selected from profiles().
	 * @return A fresh profile belonging to this parser with the selected descriptor.
	 */
	virtual std::unique_ptr<ParserProfile> make_profile_impl(ProfileInfo info) const;

	/**
	 * @brief Construct the implementation for an already selected catalog.
	 * @param catalog Owned descriptor selected from this provider's catalogs().
	 * @return A non-null root constructed with that descriptor.
	 */
	virtual std::unique_ptr<CatalogRootGetter> make_catalog_getter_impl(CatalogInfo catalog) const;
};

//using ParserProvider = std::function<
} // namespace aniparse
