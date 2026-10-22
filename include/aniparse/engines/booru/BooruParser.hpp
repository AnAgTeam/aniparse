/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/source/Parser.hpp"
#include "aniparse/engines/booru/BooruSite.hpp"

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
	/**
	 * @copydoc aniparse::Parser::suggest_getter
	 */
	GetterSuggestionResult suggest_getter(const ParsedUrl& url) const override;
	/**
	 * @brief What the source declares it can do.
	 * The descriptor's own flags plus supports_images_store, which is always set: a
	 * booru parser hands out an images getter unconditionally (@see images_getter),
	 * so the bit follows from the engine rather than from a descriptor that could
	 * forget it.
	 * @return The site's capability flags, with the image-library bit added
	 */
	ParserCapabilities capabilities() const override;
	void emplace_domains(EmplaceDomainsContext& context) const override;

	std::unique_ptr<ImagesGetter> images_getter() const override;

protected:
    /**
     * @copydoc Parser::make_profile
     */
    std::unique_ptr<ParserProfile> make_profile(ProfileInfo info) const override;

	const BooruSite& site_;
};

} // namespace aniparse::engines
