/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/CatalogManager.hpp"

namespace aniparse {

expected<uint64_t, CatalogError> CatalogManager::apply(std::string_view payload,
                                                       std::string_view signature) {
	auto decoded = decode_catalog(payload, signature, verifier_, revision_);
	if (!decoded) {
		return unexpected(decoded.error());
	}

	// Commit only after a clean decode+verify: rebuild routing, swap selectors and
	// drop stale compiled sets, then advance the revision so the next apply is
	// checked against it.
	uint64_t new_revision = decoded->revision;
	store_.refresh_domains(std::move(decoded->domains));
	if (services_) {
		if (services_->selectors) {
			services_->selectors->set(
			    std::make_shared<const html::SelectorSource>(std::move(decoded->selectors)));
		}
		if (services_->resources) {
			services_->resources->clear();
		}
	}
	revision_ = new_revision;
	return revision_;
}

} // namespace aniparse
