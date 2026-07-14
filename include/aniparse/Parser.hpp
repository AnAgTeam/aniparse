/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/FlagsBitfield.hpp"
#include "aniparse/types/Flags.hpp"
#include "aniparse/types/ParseResult.hpp"
#include "aniparse/types/ParserInfo.hpp"
#include "aniparse/types/ParsedUrl.hpp"
#include "aniparse/images/Image.hpp"
#include "aniparse/manga/Manga.hpp"

#include "aniparse/net/CancellingTask.hpp"
#include <memory>
#include <map>
#include <optional>
#include <span>

namespace aniparse {
struct ImagesGetter;
struct AnimeRootGetter;

/**
 * @brief The sink a parser declares its domains into.
 *
 * A collecting seam, not a container the parser owns: @ref Parser::emplace_domains
 * is handed one and calls @ref add_domain once per host it answers for, without
 * knowing what the caller does with them (ParserStore builds its URL-routing index
 * out of them; another consumer could just list them). The reference is valid only
 * for the duration of the emplace_domains call — a parser must not retain it.
 */
struct EmplaceDomainsContext {
	virtual ~EmplaceDomainsContext() = default;

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
};

/**
 * @brief Which kind of content a URL points at — the getter category a URL routes
 * to. @see Parser::suggest_getter, ParserStore::route_url
 */
enum class GetterSuggestionType {
	Unknown, ///< The parser does not recognize the URL (its default answer), so no getter can take it.
	Anime,   ///< An anime entry: route to Parser::animes_getter.
	Images,  ///< An image container (a post, a pool, a gallery): route to Parser::images_getter.
	Manga,   ///< A manga entry: route to Parser::mangas_getter.
	Video    ///< A standalone video, outside an anime catalog.
};

/**
 * @brief The classification of one URL.
 * @note Reserved: @ref Parser::suggest_getter returns the bare
 * GetterSuggestionType today. This wrapper exists so a classification can later
 * carry more than the category without changing that signature's shape.
 */
struct GetterSuggestionResult {
	/// The getter category the URL belongs to; Unknown when it belongs to none.
	GetterSuggestionType type;
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
 *   else is keyed by: registration, config, catalog overrides) and @ref info
 *   (name and artwork for a source list).
 * - **Reach** — @ref emplace_domains declares the hosts the source answers for,
 *   and @ref suggest_getter classifies a URL under them, which together let
 *   ParserStore route an arbitrary URL to this parser and to the right kind of
 *   getter. @ref mirrors declares the base URLs its requests actually go to.
 * - **Capability** — @ref compatibilities, the up-front answer to "does this
 *   source have a manga library / comments / adult content?", so a consumer can
 *   decide what to offer before spending a request.
 * - **Configuration** — @ref make_config derives the config a getter runs on
 *   (fresh session, parser identity, plus whatever @ref configure seeds), and
 *   @ref authenticate_context logs the source in. Login is per source, not per
 *   category: one config authorizes every getter of this parser.
 * - **Getters** — @ref mangas_getter, @ref animes_getter and @ref images_getter —
 *   the root objects that search, list and fetch. A parser implements only the
 *   categories its source has; the rest stay null.
 *
 * A parser instance is registered once (ParserStore holds it by shared_ptr) and
 * used from anywhere afterwards: every method is const and must stay so in an
 * override. A parser therefore holds no mutable state — the per-run state lives in
 * the ParserConfig / RequestorContext the caller passes to a getter, which is what
 * makes one parser instance safe to use concurrently and safe to reconfigure
 * (different mirror, different login) without cloning it.
 *
 * ### Implementing a source
 *
 * Derive and implement the four pure virtuals — @ref info, @ref identifier,
 * @ref compatibilities, @ref emplace_domains — then override the getter factory
 * for each category the source offers, returning a root getter that performs the
 * requests. Everything else has a working default: URL entry is opt-in
 * (@ref suggest_getter), as are per-request defaults (@ref configure), mirrors
 * (@ref mirrors) and login (@ref authenticate_context). Registering the finished
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
	 * @ref suggest_getter (true when a category is suggested), so a parser with
	 * URL entry only overrides suggest_getter. Override this for exotic gating.
	 * @param url Parsed URL to check
	 * @return true if the parser handles the URL, false otherwise
	 */
	virtual bool valid_for_url(const ParsedUrl& url) const;

	/**
	 * @brief Suggest which getter category a URL belongs to (Manga / Anime / …).
	 * The parser knows its own path/query scheme, so it classifies the URL to
	 * route parse_url to the right getter. Classify on whatever the URL carries —
	 * path, query (e.g. YouTube's ?v=), or host. Default: Unknown (no URL entry).
	 * @param url Parsed URL
	 * @return The getter category, or Unknown if the parser does not handle it
	 */
	virtual GetterSuggestionType suggest_getter(const ParsedUrl& url) const;

	/**
	 * @see ParserCompatibilities, @see namespace compatibilities_flags
	 * Get compatibilities, that parser can handle.
	 * For example, if the parser support anime it should
	 * set the compatibilities_flags::supports_images_search flag.
	 * @return Compatibilities of the parser
	 */
	virtual ParserCompatibilities compatibilities() const = 0;

	/**
	 * @see EmplaceDomainsContext
	 * Add parser domains to the context
	 * This function is called whenever someone wants
	 * to add the parser domains to store
	 * @param context Context to use to add domains
	 */
	virtual void emplace_domains(EmplaceDomainsContext& context) const = 0;

	/**
	 * @brief Authenticate the parser's service with the given credentials.
	 *
	 * Login is per-site (per-parser), not per-category: the returned config
	 * authorizes every getter of this parser.
	 *
	 * Contract: returns a FRESH authenticated config and does NOT mutate the
	 * passed context. Implementations log in on a new config with its own cookie
	 * jar (copy the config, clear its cookie_jar, adopt it via new_with_config,
	 * and perform the login through that context), so the caller's session is left
	 * untouched — retries and failed logins never pollute it. The caller adopts
	 * the result via RequestorContext::new_with_config. By default reports
	 * NotImplemented.
	 * @param context Client to perform HTTP requests (left unmodified)
	 * @param data Credentials to authenticate with
	 * @return Task with a new authenticated config for the caller to adopt
	 */
	virtual NetworkRequestTask<std::shared_ptr<const ParserConfig>> authenticate_context(
	    RequestorContext context,
	    AuthenticationData data);

	/**
	 * @see AuthKeys
	 * @brief Names of the credential-bearing config entries to persist.
	 *
	 * These identify which cookies / headers / url params of the authenticated
	 * config are the durable credential and should be copied into an AuthState;
	 * everything else (volatile session/anti-bot data) is left behind. Default:
	 * empty (nothing distinguished). Only cookie-/header-/param-based logins need
	 * to override this.
	 * @return The names of the durable credential entries; empty when none.
	 */
	virtual AuthKeys auth_keys() const noexcept;

	/**
	 * @see AuthState
	 * @brief Distill a persistable session from an authenticated config.
	 *
	 * Default: copy the entries named by auth_keys() out of the config. Override
	 * only for exotic credentials that a name list cannot express (e.g. a token
	 * parsed out of a response body).
	 * @param config Authenticated config to export from
	 * @return The persistable session
	 */
	virtual AuthState export_auth(const ParserConfig& config) const;

	/**
	 * @brief Ready a usable config for this parser from a base config.
	 *
	 * The single seam for deriving a config before any getter runs: copies
	 * @p base, gives the copy a fresh session (its own cookie jar is provisioned
	 * lazily on adoption, so two instances of a parser never share a login),
	 * stamps this parser's identity so request-time services keyed by parser
	 * (mirror overrides) can find it, and applies parser-specific defaults through
	 * @ref configure. Callers adopt the result via
	 * RequestorContext::new_with_config. Restoring a saved session is a separate,
	 * explicit path (assign a deserialized jar to the returned config).
	 * @param base Base config to derive from.
	 * @return A fresh derived config, or nullptr if @p base is nullptr.
	 */
	[[nodiscard]] std::shared_ptr<ParserConfig> make_config(
	    std::shared_ptr<const ParserConfig> base) const;

	/**
	 * @brief Seed parser-specific defaults onto a freshly derived config.
	 *
	 * Called by @ref make_config after the common derivation (fresh session,
	 * identity stamp), so a parser need not repeat these per request. Default:
	 * no-op. Override to set constant headers/params a parser needs on every
	 * request (e.g. an API's fixed headers). These are parser-wide, not
	 * category-specific — one config shape serves all of a parser's getters.
	 * @param config Config being readied, mutated in place.
	 */
	virtual void configure(ParserConfig& config) const;

	/**
	 * @brief The source's built-in mirrors (full base URLs), in selection order.
	 *
	 * A source-level declaration, like @ref emplace_domains(): one mirror set serves
	 * all of a parser's getters. The single source of truth a getter's fetch path
	 * (RequestorContext::base_url) and the UI picker (@ref mirror_choices) both read;
	 * a live catalog override, keyed by @ref identifier, supersedes it at request
	 * time. Default: none.
	 * @return The built-in base URLs; empty for a source with no selectable mirrors.
	 */
	[[nodiscard]] virtual std::span<const std::string_view> mirrors() const;

	/**
	 * @brief The mirror choices to show the user, with any live catalog override
	 * applied.
	 *
	 * The built-in @ref mirrors overlaid with the override carried in @p context
	 * (keyed by this parser's identity, stamped into the config by @ref make_config)
	 * — so a picker reflects the mirrors actually fetched from, not a stale list.
	 * @param context Context carrying the current mirror snapshot and parser id.
	 * @return The resolved mirror descriptors, in selection order.
	 */
	[[nodiscard]] std::vector<AltLink> mirror_choices(const RequestorContext& context) const;

	/**
	 * @brief The root of the source's image library: image containers (posts,
	 * pools, galleries), their search and their listings.
	 *
	 * A fresh getter, owned by the caller; getters are cheap and stateless, so one
	 * may be made per use rather than cached. It carries no config — the caller
	 * passes a RequestorContext (derived through @ref make_config) into each call.
	 * Default: nullptr.
	 * @return The root images getter, or nullptr if the source has no image library.
	 */
	virtual std::unique_ptr<ImagesGetter> images_getter() const;

	/**
	 * @brief The root of the source's manga library: search, latest listings, and a
	 * per-title getter that yields chapters and pages.
	 *
	 * Same ownership contract as @ref images_getter. Default: nullptr.
	 * @return The root manga getter, or nullptr if the source has no manga library.
	 */
	virtual std::unique_ptr<MangaRootGetter> mangas_getter() const;

	/**
	 * @brief The root of the source's anime library: search, latest listings, and a
	 * per-title getter that yields episodes and their video sources.
	 *
	 * Same ownership contract as @ref images_getter. Default: nullptr.
	 * @return The root anime getter, or nullptr if the source has no anime library.
	 */
	virtual std::unique_ptr<AnimeRootGetter> animes_getter() const;
};

//using ParserProvider = std::function<
} // namespace aniparse