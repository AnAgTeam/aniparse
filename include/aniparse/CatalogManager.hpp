/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Catalog.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/ParserStore.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace aniparse {

/**
 * @brief Applies verified catalogs to a ParserStore in one guarded step.
 *
 * Owns the anti-rollback state (the last applied revision) so the security
 * invariant — never apply an unverified, malformed, or non-newer catalog — lives
 * in one place instead of in every host. apply() verifies and decodes the
 * payload and, only on success, rebuilds the store's routing and advances the
 * revision. It performs no I/O: fetching, persistence and scheduling stay with
 * the host, and the crypto algorithm is the injected verifier's.
 */
class CatalogManager {
public:
	/**
	 * @param store    The store whose routing this manager refreshes; must outlive
	 *                 the manager.
	 * @param verifier The signature verifier (carries the pinned key); must outlive
	 *                 the manager.
	 * @param services Optional shared services; when given, a successful apply also
	 *                 swaps the catalog's selectors into its holder and clears its
	 *                 resource cache so compiled sets rebuild. Null = domains only.
	 */
	CatalogManager(ParserStore& store, const SignatureVerifier& verifier,
	               std::shared_ptr<const ServiceState> services = nullptr)
	    : store_(store), verifier_(verifier), services_(std::move(services)) {}

	/**
	 * @brief Verify, decode and apply a catalog payload.
	 * On success the store's routing is rebuilt from the catalog's domains and the
	 * applied revision advances; on any failure nothing changes. Never throws — a
	 * bad or hostile catalog travels through the CatalogError channel.
	 * @param payload   The exact catalog bytes that were signed.
	 * @param signature The detached signature over @p payload.
	 * @return The newly applied revision, or a CatalogError.
	 */
	[[nodiscard]] expected<uint64_t, CatalogError> apply(std::string_view payload,
	                                                     std::string_view signature);

	/// @return The last applied revision (0 if none has been applied yet).
	[[nodiscard]] uint64_t revision() const noexcept { return revision_; }

private:
	ParserStore& store_;
	const SignatureVerifier& verifier_;
	std::shared_ptr<const ServiceState> services_;
	uint64_t revision_ = 0;
};

} // namespace aniparse
