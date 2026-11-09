/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/source/overrides/SourceOverrides.hpp"
#include "aniparse/runtime/RequestContext.hpp"
#include "aniparse/source/ParserStore.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace aniparse {

class VideoExtractorStore;

/**
 * @brief Applies verified override snapshots to a ParserStore in one guarded step.
 *
 * Owns the anti-rollback state (the last applied revision) so the security
 * invariant — never apply an unverified, malformed, or non-newer overrides — lives
 * in one place instead of in every host. apply() verifies and decodes the
 * payload and, only on success, rebuilds the store's routing and advances the
 * revision. It performs no I/O: fetching, persistence and scheduling stay with
 * the host, and the crypto algorithm is the injected verifier's.
 */
class SourceOverridesManager {
public:
	/**
	 * @param store    The store whose routing this manager refreshes; must outlive
	 *                 the manager.
	 * @param verifier The signature verifier (carries the pinned key); must outlive
	 *                 the manager.
	 * @param services Optional shared services; when given, a successful apply also
	 *                 publishes a complete parser overrides generation and clears
	 *                 the shared resource cache. Null = domains only.
	 * @param extractors Optional video extractor store; when given, a successful
	 *                 apply also refreshes its routing from the overrides's
	 *                 "extractors" section. Null = extractor overrides ignored.
	 * @param extractor_services Optional extractor-facing services; when given, a
	 *                 successful apply publishes a complete extractor generation.
	 *                 The consumer feeds extractors through
	 *                 contexts built on this state, so extractor overrides (keyed
	 *                 by extractor identifier) never mix with the parser ones in
	 *                 @p services. Null = extractor request overrides ignored.
	 */
	SourceOverridesManager(ParserStore& store, const SignatureVerifier& verifier,
	               std::shared_ptr<const RuntimeServices> services = nullptr,
	               VideoExtractorStore* extractors = nullptr,
	               std::shared_ptr<const RuntimeServices> extractor_services = nullptr)
	    : store_(store), verifier_(verifier), services_(std::move(services))
	    , extractors_(extractors), extractor_services_(std::move(extractor_services)) {}

	/**
	 * @brief Verify, decode and apply an override payload.
	 * On success the store's routing is rebuilt from the overrides's domains and the
	 * applied revision advances; on any failure nothing changes. Never throws — a
	 * bad or hostile overrides travels through the OverridesError channel.
	 * @param payload   The exact overrides bytes that were signed.
	 * @param signature The detached signature over @p payload.
	 * @return The newly applied revision, or a OverridesError.
	 */
	[[nodiscard]] expected<uint64_t, OverridesError> apply(std::string_view payload,
	                                                     std::string_view signature);

	/**
	 * @brief Discard every active remote override.
	 *
	 * Rebuilds parser and extractor routing from static declarations only, publishes
	 * empty overrides generations, clears shared resource caches, and resets the
	 * anti-rollback revision to zero. In-flight
	 * operations retain their old immutable snapshots; operations begun after this
	 * call observe built-in parser and extractor fallbacks.
	 * @note This method performs no I/O and does not verify or apply a new
	 *       overrides. The caller may safely fetch and apply one afterwards.
	 */
	void reset();

	/// @return The last applied revision (0 if none has been applied yet).
	[[nodiscard]] uint64_t revision() const noexcept { return revision_; }

private:
	ParserStore& store_;
	const SignatureVerifier& verifier_;
	std::shared_ptr<const RuntimeServices> services_;
	VideoExtractorStore* extractors_;
	std::shared_ptr<const RuntimeServices> extractor_services_;
	uint64_t revision_ = 0;
};

} // namespace aniparse
