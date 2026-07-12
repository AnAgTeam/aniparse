/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Parser.hpp"
#include "aniparse/engines/BooruSite.hpp"

namespace aniparse::engines {

/**
 * @brief A booru parser driven entirely by a @ref BooruSite descriptor: identity and
 * routing come from the site's data, all behaviour from the site's engine. One
 * implementation serves every booru source — a concrete parser is this class bound to a
 * site constant (see the thin per-site subclasses). The site reference must outlive the
 * parser (site descriptors are static constants).
 */
class BooruParser : public Parser {
public:
	explicit BooruParser(const BooruSite& site) : site_(site) {}

	ParserInfo info() const override;
	std::string identifier() const override;
	GetterSuggestionType suggest_getter(const ParsedUrl& url) const override;
	ParserCompatibilities compatibilities() const override;
	void emplace_domains(EmplaceDomainsContext& context) const override;
	void configure(ParserConfig& config) const override;
	std::span<const std::string_view> mirrors() const override;
	std::unique_ptr<ImagesGetter> images_getter() const override;

	/// Fold static query-param credentials (@ref BooruStaticParamAuth) into a fresh
	/// config, or report NotImplemented for an anonymous site. No network round-trip.
	NetworkRequestTask<std::shared_ptr<const ParserConfig>> authenticate_context(
	    RequestorContext context,
	    AuthenticationData data) override;

	/// The url_params that carry the durable credential, per the site's auth model;
	/// empty for an anonymous site.
	AuthKeys auth_keys() const noexcept override;

protected:
	const BooruSite& site_;
};

} // namespace aniparse::engines
