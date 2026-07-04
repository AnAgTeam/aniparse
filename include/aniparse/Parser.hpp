/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"
#include "aniparse/images/Image.hpp"
#include "aniparse/manga/Manga.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <memory>
#include <map>

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
	 * @brief Check if the parser can handle given url
	 * @param url Url to check
	 * @return true if the parser can handle, false otherwise
	 */
	virtual bool valid_for_url(std::string_view url) const = 0;

	/**
	 * @see ParserCompatibilities, @see namespace compatibilities_flags
	 * Get compatibilies, that parser can handle.
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

	//virtual GetterSuggestionResult suggest_getter(std::string_view url) = 0;

	/**
	 * @brief Authenticate the parser's service with the given credentials.
	 *
	 * Login is per-site (per-parser), not per-category: the returned config
	 * authorizes every getter of this parser. By default reports NotImplemented.
	 * @param context Client to perform HTTP requests
	 * @param data Credentials to authenticate with
	 * @return Task with the authenticated config
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

	virtual std::unique_ptr<ImagesGetter> images_getter() const;

	virtual std::unique_ptr<MangaRootGetter> mangas_getter() const;
};

//using ParserProvider = std::function<
} // namespace aniparse