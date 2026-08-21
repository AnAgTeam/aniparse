/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/Expected.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file
 * The volatile half of a source definition — domains, mirrors, canonical
 * frontend origins and selectors —
 * as signed data that can be refreshed without shipping a new binary. Sites
 * break; when they do, the fix should be a catalog push, not a release.
 *
 * The catalog arrives over the network, so it is treated as hostile input: it is
 * verified against a pinned key before anything in it is believed, and a bad one
 * is a recoverable CatalogError, not an exception. Only data ever crosses the
 * wire — never code.
 */

namespace aniparse {

/**
 * @brief Why a fetched catalog was rejected. A hostile or malformed catalog is
 * a recoverable runtime condition (its source is untrusted), not a bug, so
 * decode_catalog reports it through this rather than throwing.
 */
enum class CatalogError {
	BadSignature,       ///< The detached signature did not verify over the payload.
	BadFormat,          ///< The payload is not valid JSON, or a required field is missing/mistyped.
	UnsupportedVersion, ///< The payload's schema_version is not the one this build reads.
	StaleRevision,      ///< The payload's revision is not newer than the last applied (anti-rollback).
};

/**
 * @brief Injected crypto seam: verifies a detached signature over the exact
 * catalog payload bytes. The implementation carries the pinned public key(s);
 * the core owns what and when to verify, the platform provides the algorithm
 * (Apple CryptoKit on iOS, OpenSSL/libsodium on desktop).
 */
struct SignatureVerifier {
	virtual ~SignatureVerifier() = default;

	/**
	 * @brief Verify a detached signature over a payload.
	 * @param payload   The exact bytes that were signed.
	 * @param signature The detached signature to check against the pinned key.
	 * @return True if the signature is valid for @p payload under the pinned key.
	 */
	[[nodiscard]] virtual bool verify(std::string_view payload,
	                                  std::string_view signature) const = 0;
};

/**
 * @brief A verified, parsed catalog. @c domains maps a parser identifier to the
 * extra domains it should route (for ParserStore::refresh_domains); @c mirrors
 * maps a parser identifier to its ordered fetch base URLs and
 * @c canonical_base_urls maps it to a public frontend origin (both for
 * MirrorSource);
 * @c extractor_domains / @c extractor_mirrors are the same pair for video
 * extractors, keyed by extractor identifier; @c selectors maps a stable selector
 * name (e.g. "example.info.description") to
 * its override CSS, a flat table fed straight into a SelectorSource. Filters
 * join later.
 */
struct CatalogData {
	uint64_t revision = 0;
	std::map<std::string, std::vector<std::string>, std::less<>> domains;
	/// Per-parser ordered fetch base URLs (for MirrorSource), keyed by the same
	/// parser identifier as @c domains. Distinct from @c domains: a routing domain
	/// only recognises a URL, a mirror is a live base URL fetched from; they may
	/// overlap (a frontend host is both) and CatalogManager unions mirror hosts
	/// into routing so listing a mirror also makes it route.
	std::map<std::string, std::vector<std::string>, std::less<>> mirrors;
	/// Per-parser canonical frontend origins. This is deliberately separate from
	/// @c mirrors: a source can fetch from an API host while sharing links, web
	/// authentication, and Referer headers against a different frontend origin.
	/// CatalogManager unions the origin host into routing like a mirror host.
	std::map<std::string, std::string, std::less<>> canonical_base_urls;
	/// Per-extractor extra routing domains (for VideoExtractorStore::refresh_domains),
	/// keyed by VideoExtractor::identifier(). Kept in separate maps from the parser
	/// ones so a parser and an extractor sharing an identifier never alias each
	/// other's overrides.
	std::map<std::string, std::vector<std::string>, std::less<>> extractor_domains;
	/// Per-extractor ordered fetch base URLs, keyed by extractor identifier; the
	/// consumer feeds them to extractors through a MirrorSourceHolder of its own
	/// (@see CatalogManager). CatalogManager unions their hosts into
	/// @c extractor_domains the same way it does for parser mirrors.
	std::map<std::string, std::vector<std::string>, std::less<>> extractor_mirrors;
	std::map<std::string, std::string, std::less<>> selectors;
};

/// The catalog schema version this build understands.
/// The catalog schema this build reads; a payload declaring anything else is
/// rejected as UnsupportedVersion rather than parsed on a guess.
inline constexpr std::int64_t catalog_schema_version = 1;

/**
 * @brief Verify a detached signature over @p payload, then parse it into a
 * CatalogData.
 *
 * Verify-then-parse over the raw bytes (never a re-serialization), so JSON
 * non-canonicality can never break the signature. Rejects a payload whose
 * schema_version is not @ref aniparse::catalog_schema_version, or whose revision is not
 * strictly greater than @p min_revision (anti-rollback). Never throws — a bad
 * or hostile catalog travels through the CatalogError channel.
 * @param payload      The exact catalog bytes that were signed.
 * @param signature    The detached signature over @p payload.
 * @param verifier     The injected crypto seam.
 * @param min_revision The last applied revision; the payload must exceed it.
 * @return The parsed catalog, or a CatalogError.
 */
[[nodiscard]] expected<CatalogData, CatalogError> decode_catalog(
    std::string_view payload, std::string_view signature,
    const SignatureVerifier& verifier, uint64_t min_revision = 0);

} // namespace aniparse
