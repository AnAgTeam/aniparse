/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/net/CookieStore.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>

namespace aniparse {
namespace {

// Netscape-format cookie line (as produced by CURLINFO_COOKIELIST):
//   domain \t include_subdomains \t path \t secure \t expires \t name \t value
// httponly cookies carry a "#HttpOnly_" prefix on the domain field. The value is
// the trailing field and is taken verbatim (tabs and all).
constexpr std::string_view httponly_prefix = "#HttpOnly_";

bool equals_ignore_case(std::string_view left, std::string_view right) {
	return std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
		return std::tolower(a) == std::tolower(b);
	});
}

bool ends_with_ignore_case(std::string_view text, std::string_view suffix) {
	return text.size() >= suffix.size()
	    && equals_ignore_case(text.substr(text.size() - suffix.size()), suffix);
}

// A cookie is expired when it carries an explicit expiry that has passed. A
// session cookie (no expiry) never expires on its own — it dies with the jar.
bool is_expired(const Cookie& cookie, std::chrono::system_clock::time_point now) {
	return cookie.expires && *cookie.expires <= now;
}

// RFC 6265 identity: two cookies are the same cookie when name, domain and path
// match, whatever their values or attributes.
bool same_identity(const Cookie& left, const Cookie& right) {
	return left.name == right.name && left.domain == right.domain && left.path == right.path;
}

} // namespace

bool cookie_domain_matches(const Cookie& cookie, std::string_view host) {
	std::string_view domain = cookie.domain;
	// The Netscape format spells the subdomain opt-in as a leading dot on the
	// domain, so honour it even when the flag field says otherwise.
	bool include_subdomains = cookie.include_subdomains;
	if (domain.starts_with('.')) {
		domain.remove_prefix(1);
		include_subdomains = true;
	}
	if (domain.empty()) {
		return false;
	}
	if (equals_ignore_case(host, domain)) {
		return true;
	}
	// Tail match, but only at a label boundary: a "example.com" cookie covers
	// "api.example.com" and must not cover "notexample.com".
	return include_subdomains && host.size() > domain.size()
	    && ends_with_ignore_case(host, domain)
	    && host[host.size() - domain.size() - 1] == '.';
}

bool cookie_path_matches(const Cookie& cookie, std::string_view path) {
	std::string_view scope = cookie.path;
	if (scope.empty() || scope == "/") {
		return true;
	}
	if (path.empty()) {
		path = "/";
	}
	if (!path.starts_with(scope)) {
		return false;
	}
	// Prefix must land on a segment boundary, so "/foo" covers "/foo" and
	// "/foo/bar" but never "/foobar".
	return path.size() == scope.size() || scope.back() == '/' || path[scope.size()] == '/';
}

std::optional<Cookie> parse_netscape_line(std::string_view line) {
	constexpr size_t field_count = 7;

	std::array<std::string_view, field_count> fields;
	size_t count = 0;
	size_t start = 0;

	while (count < field_count) {
		// Last field (the value) is the remainder of the line, tabs and all.
		size_t tab = (count == field_count - 1) ? std::string_view::npos : line.find('\t', start);
		if (tab == std::string_view::npos) {
			fields[count++] = line.substr(start);
			break;
		}
		fields[count++] = line.substr(start, tab - start);
		start = tab + 1;
	}

	if (count != field_count) {
		return std::nullopt;
	}

	Cookie cookie;
	std::string_view domain = fields[0];
	if (domain.starts_with(httponly_prefix)) {
		cookie.http_only = true;
		domain.remove_prefix(httponly_prefix.size());
	}
	cookie.domain             = std::string(domain);
	cookie.include_subdomains = fields[1] == "TRUE";
	cookie.path               = std::string(fields[2]);
	cookie.secure             = fields[3] == "TRUE";

	long long expires_unix = 0;
	std::from_chars(fields[4].data(), fields[4].data() + fields[4].size(), expires_unix);
	if (expires_unix != 0) {
		cookie.expires = std::chrono::system_clock::time_point(std::chrono::seconds(expires_unix));
	}

	cookie.name  = std::string(fields[5]);
	cookie.value = std::string(fields[6]);
	return cookie;
}

std::string to_netscape_line(const Cookie& cookie) {
	long long expires_unix = cookie.expires
	    ? std::chrono::duration_cast<std::chrono::seconds>(cookie.expires->time_since_epoch()).count()
	    : 0;

	std::string line;
	if (cookie.http_only) {
		line += httponly_prefix;
	}
	line += cookie.domain;
	line += '\t';
	line += cookie.include_subdomains ? "TRUE" : "FALSE";
	line += '\t';
	line += cookie.path;
	line += '\t';
	line += cookie.secure ? "TRUE" : "FALSE";
	line += '\t';
	line += std::to_string(expires_unix);
	line += '\t';
	line += cookie.name;
	line += '\t';
	line += cookie.value;
	return line;
}

std::optional<Cookie> CookieStore::find(std::string_view name) const {
	std::lock_guard lock(mutex_);
	auto it = std::ranges::find(cookies_, name, &Cookie::name);
	if (it == cookies_.end()) {
		return std::nullopt;
	}
	return *it;
}

std::vector<Cookie> CookieStore::all() const {
	std::lock_guard lock(mutex_);
	return cookies_;
}

std::vector<Cookie> CookieStore::cookies_for(std::string_view host,
                                             std::string_view path,
                                             bool secure_transport) {
	const auto now = std::chrono::system_clock::now();

	std::lock_guard lock(mutex_);
	// Prune on read: a long-lived jar would otherwise accumulate every expired
	// cookie a source ever set, and this is the only path walked often enough to
	// notice.
	std::erase_if(cookies_, [&](const Cookie& cookie) { return is_expired(cookie, now); });

	std::vector<Cookie> matching;
	for (const Cookie& cookie : cookies_) {
		if (cookie.secure && !secure_transport) {
			continue;
		}
		if (cookie_domain_matches(cookie, host) && cookie_path_matches(cookie, path)) {
			matching.push_back(cookie);
		}
	}
	return matching;
}

void CookieStore::set(const Cookie& cookie) {
	std::lock_guard lock(mutex_);
	auto it = std::ranges::find_if(cookies_,
	                               [&](const Cookie& stored) { return same_identity(stored, cookie); });
	if (it != cookies_.end()) {
		*it = cookie;
	} else {
		cookies_.push_back(cookie);
	}
}

void CookieStore::clear() {
	std::lock_guard lock(mutex_);
	cookies_.clear();
}

std::vector<std::string> CookieStore::serialize() const {
	std::lock_guard lock(mutex_);
	std::vector<std::string> lines;
	lines.reserve(cookies_.size());
	for (const Cookie& cookie : cookies_) {
		lines.push_back(to_netscape_line(cookie));
	}
	return lines;
}

std::optional<Cookie> MemoryCookieJar::find_cookie(std::string_view name) const {
	return store_.find(name);
}

std::vector<Cookie> MemoryCookieJar::cookies() const {
	return store_.all();
}

void MemoryCookieJar::set_cookie(const Cookie& cookie) {
	store_.set(cookie);
}

void MemoryCookieJar::clear() {
	store_.clear();
}

std::vector<std::string> MemoryCookieJar::serialize() const {
	return store_.serialize();
}

void MemoryCookieJar::deserialize(std::span<std::string> cookies) {
	store_.deserialize(cookies);
}

void CookieStore::deserialize(std::span<std::string> lines) {
	std::vector<Cookie> restored;
	restored.reserve(lines.size());
	for (const std::string& line : lines) {
		if (auto cookie = parse_netscape_line(line)) {
			restored.push_back(std::move(*cookie));
		}
	}

	std::lock_guard lock(mutex_);
	cookies_ = std::move(restored);
}
} // namespace aniparse
