/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"
#include "aniparse/ParsedUrl.hpp"
#include "aniparse/images/Image.hpp"
#include "aniparse/manga/Manga.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <memory>
#include <map>
#include <span>

namespace aniparse {
struct AsyncReleaseGetter;
struct ImagesGetter;

using ParseFlags = FlagsBitfield<64, struct ParseFlagsTag>;

namespace parse_flags {
constexpr auto supports_inplace_get = ParseFlags::make_bit(0);

constexpr ParseFlags default_flags;
} // namespace parse_flags

struct ParseContext {
	std::string url;

	std::map<std::string, std::string> user_args;
};

struct ParseQueryResult {

	ParseFlags flags = parse_flags::default_flags;
};

struct ParserCompatibilities {
	std::string primary_language;
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};

template <typename T>
struct ParseResult {
	T data;
	ParseQueryResult meta;
};

struct EmplaceDomainsContext {
	virtual ~EmplaceDomainsContext() = default;

	virtual void add_domain(std::string_view domain) = 0;
};

enum class GetterSuggestionType {
	Unknown,
	Anime,
	Images,
	Manga,
	Video
};

struct GetterSuggestionResult {
	GetterSuggestionType type;
};

struct Parser {
	virtual ~Parser() = default;

	/**
	 * @return Name/display name of the parser
	 */
	virtual std::string name() const = 0;

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

	//virtual ParseResult<std::unique_ptr<AsyncReleaseGetter>> async_release_getter(ParseContext& ctx);

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
	 * A source-level declaration, like @ref emplace_domains: one mirror set serves
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

	virtual std::unique_ptr<ImagesGetter> images_getter() const;

	virtual std::unique_ptr<MangaRootGetter> mangas_getter() const;
};

//using ParserProvider = std::function<
} // namespace aniparse