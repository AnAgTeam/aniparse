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
	std::string name;   ///< Cookie name, the key a request sends it under.
	std::string value;  ///< Cookie value, verbatim (no encoding is applied or assumed).
	std::string domain; ///< Host the cookie belongs to; sent only to that host (@see include_subdomains).
	std::string path = "/"; ///< Path prefix the cookie is scoped to; "/" is the whole host.
	/// nullopt means a session cookie (no explicit expiry; dies with the session).
	std::optional<std::chrono::system_clock::time_point> expires;
	bool secure    = false; ///< Send only over HTTPS.
	bool http_only = false; ///< Withheld from page scripts; carried by HTTP requests as usual.
	/// Netscape tail-match: also send to subdomains of `domain`.
	bool include_subdomains = false;

	/// Two cookies are equal when every field above matches.
	friend bool operator==(const Cookie&, const Cookie&) = default;
};

/**
 * @brief The cookie store one parser's session is kept in — the seam between the
 * neutral @ref Cookie model and whatever store the HTTP backend actually reads.
 *
 * A jar is never constructed directly: it is minted by the client backend
 * (ClientContext::make_cookie_jar) and held by a ParserConfig, so every request made
 * with that config reads and writes the same cookies while a config derived for
 * another parser gets a jar of its own. Mutations therefore land in the transport's
 * live store, not in a copy — a login performed over one request is visible to the
 * next.
 *
 * Requests of a parser can run concurrently, so an implementation must tolerate
 * concurrent access.
 */
class CookieJar {
public:
	virtual ~CookieJar() = default;

	/**
	 * @brief The first cookie carrying `name`, or nullopt if none is stored.
	 * @param name Cookie name to look for
	 * @return The matching cookie, or nullopt when the jar holds none. Names are not
	 *         unique across domains/paths; the first match wins, so a caller that needs
	 *         a specific domain should scan @ref cookies() instead.
	 */
	[[nodiscard]] virtual std::optional<Cookie> find_cookie(std::string_view name) const = 0;

	/**
	 * @brief Every cookie currently in the jar.
	 * @return A snapshot of the stored cookies, in no guaranteed order
	 */
	[[nodiscard]] virtual std::vector<Cookie> cookies() const = 0;

	/**
	 * @brief Store `cookie`, replacing any existing one with the same name/domain/path.
	 * @param cookie The cookie to store
	 */
	virtual void set_cookie(const Cookie& cookie) = 0;

	/// Drop every cookie, ending the session the jar was holding.
	virtual void clear() = 0;

	/**
	 * @brief Export the jar as opaque lines that @ref deserialize can restore.
	 *
	 * The persistence path: the lines are what a host application writes to disk to
	 * keep a login across runs. Their format is the backend's business — a caller must
	 * treat them as opaque and hand them back unchanged.
	 * @return One line per stored cookie
	 */
	[[nodiscard]] virtual std::vector<std::string> serialize() const = 0;

	/**
	 * @brief Replace the jar's contents with a previously serialized set.
	 *
	 * Restores a saved session: the jar is cleared first, so what remains is exactly
	 * @p cookies. Restoring into the jar of a freshly derived config is how a stored
	 * login is put back into use.
	 * @param cookies Lines produced by @ref serialize()
	 */
	virtual void deserialize(std::span<std::string> cookies)         = 0;
};
} // namespace aniparse