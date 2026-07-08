/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/CatalogManager.hpp"

#include <algorithm>
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
} // namespace

expected<uint64_t, CatalogError> CatalogManager::apply(std::string_view payload,
                                                       std::string_view signature) {
	auto decoded = decode_catalog(payload, signature, verifier_, revision_);
	if (!decoded) {
		return unexpected(decoded.error());
	}

	// A mirror host is also a routable host: union each mirror's host into that
	// parser's routing domains, so listing a mirror makes it route too (a URL
	// pasted from a live mirror resolves without repeating it under "domains").
	auto domains = std::move(decoded->domains);
	for (const auto& [id, urls] : decoded->mirrors) {
		std::vector<std::string>& hosts = domains[id];
		for (const std::string& url : urls) {
			std::string host(host_of(url));
			if (std::find(hosts.begin(), hosts.end(), host) == hosts.end()) {
				hosts.push_back(std::move(host));
			}
		}
	}

	// Commit only after a clean decode+verify: rebuild routing, swap mirrors and
	// selectors, drop stale compiled sets, then advance the revision so the next
	// apply is checked against it.
	uint64_t new_revision = decoded->revision;
	store_.refresh_domains(std::move(domains));
	if (services_) {
		if (services_->mirrors) {
			services_->mirrors->set(
			    std::make_shared<const MirrorSource>(std::move(decoded->mirrors)));
		}
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
