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
	/**
	 * @brief What the source declares it can do.
	 * The descriptor's own flags plus supports_images_store, which is always set: a
	 * booru parser hands out an images getter unconditionally (@see images_getter),
	 * so the bit follows from the engine rather than from a descriptor that could
	 * forget it.
	 * @return The site's capability flags, with the image-library bit added
	 */
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
