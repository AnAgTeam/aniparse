/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/catalog/CatalogManager.hpp"
#include "aniparse/video/VideoExtractor.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aniparse {

namespace {
// The host of a base URL, e.g. "https://x8.example:443/path" -> "x8.example".
// Mirrors are full base URLs but routing matches on the bare host, so a mirror
// host is unioned into routing as just its host.
std::string_view host_of(std::string_view url) noexcept {
	if (const size_t scheme = url.find("://"); scheme != std::string_view::npos) {
		url.remove_prefix(scheme + 3);
	}
	url = url.substr(0, url.find('/'));
	return url.substr(0, url.find(':'));
}

// A mirror host is also a routable host: union each mirror's host into that
// entry's routing domains, so listing a mirror makes it route too (a URL pasted
// from a live mirror resolves without repeating it under "domains").
void union_mirror_hosts(
    std::map<std::string, std::vector<std::string>, std::less<>>& domains,
    const std::map<std::string, std::vector<std::string>, std::less<>>& mirrors) {
	for (const auto& [id, urls] : mirrors) {
		std::vector<std::string>& hosts = domains[id];
		for (const std::string& url : urls) {
			std::string host(host_of(url));
			if (std::find(hosts.begin(), hosts.end(), host) == hosts.end()) {
				hosts.push_back(std::move(host));
			}
		}
	}
}

// A canonical frontend origin is also routable: it is the address used for
// human-facing links and interactive web authentication, so accept pasted URLs
// from it without requiring a duplicate domains entry in the catalog.
void union_canonical_hosts(
    std::map<std::string, std::vector<std::string>, std::less<>>& domains,
    const std::map<std::string, std::string, std::less<>>& canonical_base_urls) {
	for (const auto& [id, url] : canonical_base_urls) {
		std::vector<std::string>& hosts = domains[id];
		std::string host(host_of(url));
		if (std::find(hosts.begin(), hosts.end(), host) == hosts.end()) {
			hosts.push_back(std::move(host));
		}
	}
}
} // namespace

expected<uint64_t, CatalogError> CatalogManager::apply(std::string_view payload,
                                                       std::string_view signature) {
	auto decoded = decode_catalog(payload, signature, verifier_, revision_);
	if (!decoded) {
		return unexpected(decoded.error());
	}

	auto domains = std::move(decoded->domains);
	union_mirror_hosts(domains, decoded->mirrors);
	union_canonical_hosts(domains, decoded->canonical_base_urls);

	// Commit only after a clean decode+verify: rebuild routing, swap mirrors and
	// selectors, drop stale compiled sets, then advance the revision so the next
	// apply is checked against it.
	uint64_t new_revision = decoded->revision;
	store_.refresh_domains(std::move(domains));
	if (extractors_) {
		auto extractor_domains = std::move(decoded->extractor_domains);
		union_mirror_hosts(extractor_domains, decoded->extractor_mirrors);
		extractors_->refresh_domains(std::move(extractor_domains));
	}
	if (services_) {
		if (services_->mirrors) {
			services_->mirrors->set(
			    std::make_shared<const MirrorSource>(std::move(decoded->mirrors),
			                                         std::move(decoded->canonical_base_urls)));
		}
		if (services_->selectors) {
			services_->selectors->set(
			    std::make_shared<const html::SelectorSource>(std::move(decoded->selectors)));
		}
		if (services_->resources) {
			services_->resources->clear();
		}
	}
	if (extractor_services_ && extractor_services_->mirrors) {
		extractor_services_->mirrors->set(
		    std::make_shared<const MirrorSource>(std::move(decoded->extractor_mirrors)));
	}
	revision_ = new_revision;
	return revision_;
}

} // namespace aniparse
