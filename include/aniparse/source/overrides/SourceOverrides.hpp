/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/Expected.hpp"
#include "aniparse/source/overrides/ProfileOverrides.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

/**
 * @file
 * The volatile half of a source definition — domains, mirrors, canonical
 * frontend origins, selectors and regex patterns —
 * as signed data that can be refreshed without shipping a new binary. Sites
 * break; when they do, the fix should be an override update, not a release.
 *
 * The override snapshot arrives over the network, so it is treated as hostile input: it is
 * verified against a pinned key before anything in it is believed, and a bad one
 * is a recoverable OverridesError, not an exception. Only data ever crosses the
 * wire — never code.
 */

namespace aniparse {

/**
 * @brief Why a fetched override snapshot was rejected. A hostile or malformed override snapshot is
 * a recoverable runtime condition (its source is untrusted), not a bug, so
 * decode_overrides reports it through this rather than throwing.
 */
enum class OverridesError {
	BadSignature,       ///< The detached signature did not verify over the payload.
	BadFormat,          ///< The payload is not valid JSON, or a required field is missing/mistyped.
	UnsupportedVersion, ///< The payload's schema_version is not the one this build reads.
	StaleRevision,      ///< The payload's revision is not newer than the last applied (anti-rollback).
};

/**
 * @brief Injected crypto seam: verifies a detached signature over the exact
 * overrides payload bytes. The implementation carries the pinned public key(s);
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

protected:
	SignatureVerifier() = default;
	SignatureVerifier(const SignatureVerifier&) = default;
	SignatureVerifier(SignatureVerifier&&) = default;
	SignatureVerifier& operator=(const SignatureVerifier&) = default;
	SignatureVerifier& operator=(SignatureVerifier&&) = default;
};

/**
 * @brief A verified overrides of parser domains, profile origins, selectors and patterns.
 *
 * Parser request origins are keyed by parser and profile ID. Extractor domains
 * and mirrors are keyed by extractor ID in separate maps. Selectors retain their
 * fully qualified flat keys; regex patterns are scoped by owner ID.
 */
struct SourceOverrides {
	uint64_t revision = 0;
	std::map<std::string, std::vector<std::string>, std::less<>> domains;
	/// Profile settings keyed by (parser ID, profile ID); no cross-profile fallback.
	std::map<std::pair<std::string, std::string>, ProfileOverrides> profiles;
	/// Per-extractor extra routing domains (for VideoExtractorStore::refresh_domains),
	/// keyed by VideoExtractor::identifier(). Kept in separate maps from the parser
	/// ones so a parser and an extractor sharing an identifier never alias each
	/// other's overrides.
	std::map<std::string, std::vector<std::string>, std::less<>> extractor_domains;
	/// Per-extractor ordered fetch base URLs, keyed by extractor identifier; the
	/// consumer feeds them to extractors through an OverridesStore of its own
	/// (@see SourceOverridesManager). SourceOverridesManager unions their hosts into
	/// @c extractor_domains the same way it does for parser mirrors.
	std::map<std::string, std::vector<std::string>, std::less<>> extractor_mirrors;
	std::map<std::string, std::string, std::less<>> selectors;
	/// Per-parser regex pattern overrides: parser identifier -> short pattern key
	/// (e.g. "info.title") -> pattern text, fed into a RegexSource (@see
	/// aniparse/utility/RegexSource.hpp). Scoped by parser id so one parser's
	/// hotfix cannot name another parser's pattern.
	std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>> patterns;
	/// Per-extractor regex pattern overrides, keyed by extractor identifier; the
	/// consumer feeds them to extractors through an OverridesStore of its own
	/// (@see SourceOverridesManager). Kept separate from @c patterns for the same
	/// anti-aliasing reason as @c extractor_domains.
	std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>> extractor_patterns;
};

/// The overrides schema version this build understands.
/// The overrides schema this build reads; a payload declaring anything else is
/// rejected as UnsupportedVersion rather than parsed on a guess.
inline constexpr std::int64_t overrides_schema_version = 1;

/**
 * @brief Verify a detached signature over @p payload, then parse it into a
 * SourceOverrides.
 *
 * Verify-then-parse over the raw bytes (never a re-serialization), so JSON
 * non-canonicality can never break the signature. Rejects a payload whose
 * schema_version is not @ref aniparse::overrides_schema_version, or whose revision is not
 * strictly greater than @p min_revision (anti-rollback). Never throws — a bad
 * or hostile overrides travels through the OverridesError channel.
 * @param payload      The exact overrides bytes that were signed.
 * @param signature    The detached signature over @p payload.
 * @param verifier     The injected crypto seam.
 * @param min_revision The last applied revision; the payload must exceed it.
 * @return The parsed overrides, or a OverridesError.
 */
[[nodiscard]] expected<SourceOverrides, OverridesError> decode_overrides(
    std::string_view payload, std::string_view signature,
    const SignatureVerifier& verifier, uint64_t min_revision = 0);

} // namespace aniparse
