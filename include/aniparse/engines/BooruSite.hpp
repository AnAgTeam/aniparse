/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Flags.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace aniparse::engines {

struct BooruEngine;

/**
 * @brief Static query-param credentials — a user id + api key pair carried as query
 * params. The username is stamped into @ref user_param and the password into @ref key_param of the
 * config's url_params, with no network round-trip: booru credentials are static
 * parameters that ride on every request. @see BooruParser::authenticate_context
 */
struct BooruStaticParamAuth {
	std::string_view user_param; ///< url_param the username (user id) is stamped into.
	std::string_view key_param;  ///< url_param the password (api key) is stamped into.
};

/**
 * @brief The data descriptor of one booru source: its identity and routing data plus
 * a pointer to the shared @ref BooruEngine that supplies all behaviour. This is the
 * "site = data" half of the split — a concrete booru source is one of these constants
 * (built-in default) that a signed catalog can override or, bound to an already-shipped
 * engine, extend. Adding a site on an existing engine is a descriptor + a registration,
 * no new getter or parser code. @see BooruParser
 */
struct BooruSite {
	/// Stable routing/config key (Parser::identifier). Distinguishes two sites whose
	/// post ids collide, and keys any catalog override (domains/mirrors) for this site.
	std::string_view identifier;
	/// Human-facing display name (ParserInfo::name); may equal @ref identifier.
	std::string_view name;
	/// Primary content language as a BCP-47 tag ("en", "ru", "multi").
	std::string_view primary_language;
	/// The hosts this source owns, for URL routing (Parser::emplace_domains). The
	/// built-in set; a catalog refresh unions volatile domains onto it.
	std::span<const std::string_view> domains;
	/// The API fetch host(s) — the built-in mirror set (Parser::mirrors), routed
	/// through RequestorContext::base_url so a catalog override can move them. Distinct
	/// from @ref domains (URL-routing ownership): two sites running the same engine
	/// differ here (their own API hosts).
	std::span<const std::string_view> api_hosts;
	/// The Referer to attach to every media fetch (image / poster / preview), when the
	/// source hotlink-protects its media — the img host 302s a bare GET, and a Referer
	/// of the site returns the bytes. nullopt for sources that serve media to a bare
	/// GET. Site data, not engine behaviour: two sites on one engine carry their own.
	std::optional<std::string_view> media_referer;
	/// Accessor to the shared engine instance (a static per-family singleton). A
	/// function pointer, so the descriptor stays a constant expression.
	const BooruEngine& (*engine)();
	/// Parser-level capability flags (Parser::compatibilities) — e.g. whether the
	/// source hosts adult content. A datum, since a SFW booru would clear adult_source.
	/// Lists only what the site adds: BooruParser always advertises
	/// supports_images_store on top of this, because every booru parser hands out an
	/// images getter, so a descriptor cannot forget the bit and need not repeat it.
	CompatibilitiesFlags compatibilities;
	/// The credential model, or nullopt for an anonymous source (reads need no login).
	std::optional<BooruStaticParamAuth> auth;
};

} // namespace aniparse::engines
