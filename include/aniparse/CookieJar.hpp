/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <chrono>
#include <span>
#include <vector>
#include <string>
#include <optional>

namespace aniparse {

/// A single HTTP cookie with the fields common to the Netscape cookie format and
/// Foundation's NSHTTPCookie, so a jar can round-trip through either backend.
struct Cookie {
	std::string name;
	std::string value;
	std::string domain;
	std::string path = "/";
	/// nullopt means a session cookie (no explicit expiry; dies with the session).
	std::optional<std::chrono::system_clock::time_point> expires;
	bool secure    = false;
	bool http_only = false;
	/// Netscape tail-match: also send to subdomains of `domain`.
	bool include_subdomains = false;

	friend bool operator==(const Cookie&, const Cookie&) = default;
};

class CookieJar {
public:
	virtual ~CookieJar() = default;

	/// The first cookie carrying `name`, or nullopt if none is stored.
	[[nodiscard]] virtual std::optional<Cookie> find_cookie(std::string_view name) const = 0;

	/// Every cookie currently in the jar.
	[[nodiscard]] virtual std::vector<Cookie> cookies() const = 0;

	/// Store `cookie`, replacing any existing one with the same name/domain/path.
	virtual void set_cookie(const Cookie& cookie) = 0;

	virtual void clear() = 0;

	[[nodiscard]] virtual std::vector<std::string> serialize() const = 0;
	virtual void deserialize(std::span<std::string> cookies)         = 0;
};
} // namespace aniparse