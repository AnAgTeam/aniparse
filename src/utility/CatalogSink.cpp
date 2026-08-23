/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/utility/CatalogSink.hpp"

namespace aniparse {

CatalogSink::CatalogSink(std::string owner_id)
    : owner_id_(std::move(owner_id)) {}

void CatalogSink::set_canonical_base_url(std::string url) {
	if (canonical_base_url_.has_value()) {
		throw std::logic_error(fmt::format(
		    "catalog defaults of '{}': canonical_base_url set twice", owner_id_));
	}
	canonical_base_url_ = std::move(url);
}

void CatalogSink::record_all(RecordedSet& into, const RecordedSet& recorded,
                             std::string_view channel) {
	for (const auto& [key, literal] : recorded) {
		if (!into.emplace(key, literal).second) {
			throw std::logic_error(fmt::format(
			    "key '{}' collides across the owner's {} sets", key, channel));
		}
	}
}

} // namespace aniparse
