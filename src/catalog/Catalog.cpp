/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/catalog/Catalog.hpp"
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

	// Read a string array under an entry (domains / mirrors), dropping
	// any non-string element.
	auto read_urls = [](const boost::json::object& fields, const char* key) {
		std::vector<std::string> out;
		if (const boost::json::array* list = json::array_field(fields, key)) {
			for (const boost::json::value& item : *list) {
				if (const boost::json::string* url = item.if_string()) {
					out.emplace_back(url->c_str(), url->size());
				}
			}
		}
		return out;
	};

	// Read owner domains and patterns, plus optional extractor mirrors, into the
	// given maps; entries without a fields object, and empty lists, are skipped. Both
	// the "parsers" and the "extractors" sections share the domains/mirrors/patterns
	// shape; parser origins are read separately from profiles. "patterns" is a nested
	// short-key -> pattern-text object fed into a RegexSource.
	auto read_section = [&read_urls](
	                        const boost::json::object& section, auto& domains,
	                        std::map<std::string, std::vector<std::string>, std::less<>>* mirrors,
	                        auto& patterns) {
		for (const boost::json::key_value_pair& entry : section) {
			const boost::json::object* fields = entry.value().if_object();
			if (!fields) {
				continue;
			}
			std::string id(entry.key());
			std::vector<std::string> entry_domains = read_urls(*fields, "domains");
			if (!entry_domains.empty()) {
				domains.emplace(id, std::move(entry_domains));
			}
			if (mirrors) {
				std::vector<std::string> entry_mirrors = read_urls(*fields, "mirrors");
				if (!entry_mirrors.empty()) {
					mirrors->emplace(id, std::move(entry_mirrors));
				}
			}
			if (const boost::json::object* entry_patterns = json::object_field(*fields, "patterns")) {
				std::map<std::string, std::string, std::less<>> owner_patterns;
				for (const boost::json::key_value_pair& pattern : *entry_patterns) {
					if (const boost::json::string* text = pattern.value().if_string()) {
						owner_patterns.emplace(std::string(pattern.key()),
						                       std::string(text->c_str(), text->size()));
					}
				}
				if (!owner_patterns.empty()) {
					patterns.emplace(std::move(id), std::move(owner_patterns));
				}
			}
		}
	};

	if (const boost::json::object* parsers = json::object_field(*root, "parsers")) {
		read_section(*parsers, data.domains, nullptr, data.patterns);
		// Parser request origins are declared exclusively inside profiles.
		for (const auto& owner : *parsers) {
			const auto* fields = owner.value().if_object();
			if (!fields) {
				continue;
			}
			const auto* profiles_value = fields->if_contains("profiles");
			if (!profiles_value) {
				continue;
			}
			const auto* profiles = profiles_value->if_object();
			if (!profiles) {
				return unexpected(CatalogError::BadFormat);
			}
			for (const auto& entry : *profiles) {
				const auto* profile = entry.value().if_object();
				if (entry.key().empty() || !profile) {
					return unexpected(CatalogError::BadFormat);
				}
				const auto key = std::make_pair(std::string(owner.key()), std::string(entry.key()));
				if (const auto* mirrors = profile->if_contains("mirrors")) {
					if (!mirrors->is_array()) {
						return unexpected(CatalogError::BadFormat);
					}
					for (const auto& url : mirrors->as_array()) {
						if (!url.is_string() || url.as_string().empty()) {
							return unexpected(CatalogError::BadFormat);
						}
					}
					data.profile_mirrors.emplace(key, read_urls(*profile, "mirrors"));
				}
				if (const auto* origin = profile->if_contains("canonical_base_url")) {
					if (!origin->is_string() || origin->as_string().empty()) {
						return unexpected(CatalogError::BadFormat);
					}
					data.profile_canonical_base_urls.emplace(key, std::string(origin->as_string()));
				}
			}
		}

	}

	if (const boost::json::object* extractors = json::object_field(*root, "extractors")) {
		read_section(*extractors, data.extractor_domains, &data.extractor_mirrors,
		             data.extractor_patterns);
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
