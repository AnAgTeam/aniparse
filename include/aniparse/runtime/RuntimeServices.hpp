/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/HttpClient.hpp"
#include "aniparse/runtime/Logger.hpp"
#include "aniparse/cache/ResourceCache.hpp"
#include "aniparse/html/SelectorSource.hpp"
#include "aniparse/utility/RegexSource.hpp"
#include "aniparse/source/overrides/MirrorSource.hpp"
#include <memory>

namespace aniparse {

/**
 * @brief The per-parser-invariant services a RequestContext runs on: the HTTP
 * client, logger, compiled-resource cache and CSS selector overrides.
 *
 * Grouped into one handle so the context takes (services, config) instead of a
 * parameter list that grows with every new shared service. Held by
 * shared_ptr<const> and shared across contexts derived via new_with_config (a
 * no-alloc pointer swap) / new_with_logger. Only @ref client is required; the
 * rest default in (an empty resource cache, an empty selector source). New
 * shared services are added HERE, not as constructor parameters.
 *
 * The bundle itself is immutable once a context holds it: swapping one service out
 * (new_with_logger) copies the bundle rather than writing into the shared one, so a
 * context never observes another context's services changing under it. The services
 * live as long as any context referring to them — a context keeps the client, the
 * cache and the logger alive, and the caller need not outlive the contexts it made.
 * The services (the client above all) are used concurrently by every context derived
 * from them.
 */
struct RuntimeServices {
	/// Required — the context throws if this is null.
	std::shared_ptr<HttpClient> client;
	/// Optional; null = no logging.
	std::shared_ptr<Logger> logger = nullptr;
	/// Optional; a fresh empty cache is filled in when null.
	std::shared_ptr<ResourceCache> resources = nullptr;
	/// Optional swappable selector-source holder; null reads as an empty source
	/// (all built-in selectors). A catalog apply set()s a new source into it.
	std::shared_ptr<html::SelectorSourceHolder> selectors = nullptr;
	/// Optional swappable regex-source holder; null reads as an empty source
	/// (all built-in pattern literals). A catalog apply set()s a new source into
	/// it, keyed by parser identifier — or by extractor identifier for a state
	/// built to feed video extractors.
	std::shared_ptr<RegexSourceHolder> patterns = nullptr;
	/// Optional swappable mirror-source holder; null reads as an empty source
	/// (every parser falls back to its built-in mirrors). A catalog apply set()s a
	/// new source into it, keyed by parser identifier. A consumer that runs video
	/// extractors keys overrides by extractor identifier instead, by building the
	/// extractor-facing contexts on a state whose holder carries the extractor
	/// mirror source.
	std::shared_ptr<MirrorSourceHolder> mirrors = nullptr;
};

} // namespace aniparse
