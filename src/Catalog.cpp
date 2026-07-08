/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Catalog.hpp"
#include "aniparse/json/Json.hpp"

#include <boost/json.hpp>

namespace aniparse {

expected<CatalogData, CatalogError> decode_catalog(
    std::string_view payload, std::string_view signature,
    const SignatureVerifier& verifier, uint64_t min_revision) {
	// Verify the raw bytes first; never parse an unverified payload.
	if (!verifier.verify(payload, signature)) {
		return unexpected(CatalogError::BadSignature);
	}

	// Boost.JSON throws on malformed input; fold that into the error channel.
	boost::json::value parsed;
	try {
		parsed = boost::json::parse(payload);
	} catch (const std::exception&) {
		return unexpected(CatalogError::BadFormat);
	}

	const boost::json::object* root = parsed.if_object();
	if (!root) {
		return unexpected(CatalogError::BadFormat);
	}

	const boost::json::value* version = root->if_contains("schema_version");
	if (!version || !version->is_number()) {
		return unexpected(CatalogError::BadFormat);
	}
	if (json::integer(*root, "schema_version") != catalog_schema_version) {
		return unexpected(CatalogError::UnsupportedVersion);
	}

	const boost::json::value* revision = root->if_contains("revision");
	if (!revision || !revision->is_number()) {
		return unexpected(CatalogError::BadFormat);
	}

	CatalogData data;
	data.revision = static_cast<uint64_t>(json::integer(*root, "revision"));
	if (data.revision <= min_revision) {
		return unexpected(CatalogError::StaleRevision);
	}

	if (const boost::json::object* parsers = json::object_field(*root, "parsers")) {
		for (const boost::json::key_value_pair& entry : *parsers) {
			const boost::json::object* fields = entry.value().if_object();
			if (!fields) {
				continue;
			}
			std::vector<std::string> domains;
			if (const boost::json::array* list = json::array_field(*fields, "domains")) {
				for (const boost::json::value& item : *list) {
					if (const boost::json::string* domain = item.if_string()) {
						domains.emplace_back(domain->c_str(), domain->size());
					}
				}
			}
			if (!domains.empty()) {
				data.domains.emplace(std::string(entry.key()), std::move(domains));
			}
		}
	}

	// Selectors are a flat name -> CSS table, fed straight into a SelectorSource.
	if (const boost::json::object* selectors = json::object_field(*root, "selectors")) {
		for (const boost::json::key_value_pair& entry : *selectors) {
			if (const boost::json::string* css = entry.value().if_string()) {
				data.selectors.emplace(std::string(entry.key()),
				                       std::string(css->c_str(), css->size()));
			}
		}
	}

	return data;
}

} // namespace aniparse
