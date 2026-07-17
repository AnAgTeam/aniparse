/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/net/CookieJar.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file
 * The backend-neutral half of cookie handling: the Netscape line codec and an
 * in-memory store that answers "which cookies go on this URL". Nothing here names
 * a transport, so a backend that has no cookie store of its own (NSURLSession
 * binds one store per session, which cannot express the per-parser jars this
 * library hands out) can own one, and a backend that does (curl) still shares the
 * codec — which is what keeps a serialized jar readable across a backend swap.
 */

namespace aniparse {

/**
 * @brief Parse one Netscape-format cookie line.
 *
 * The format libcurl reads and writes (CURLINFO_COOKIELIST), tab-separated:
 * `domain \t include_subdomains \t path \t secure \t expires \t name \t value`,
 * where an httponly cookie carries a `#HttpOnly_` prefix on the domain field and
 * `expires` is a unix timestamp with 0 meaning a session cookie. The value is the
 * trailing field and is taken verbatim, tabs and all.
 * @param line One serialized cookie line
 * @return The parsed cookie, or nullopt if the line does not carry all 7 fields
 */
[[nodiscard]] std::optional<Cookie> parse_netscape_line(std::string_view line);

/**
 * @brief Serialize a cookie to a Netscape-format line. The inverse of
 * @ref parse_netscape_line — @see it for the field layout.
 * @param cookie The cookie to serialize
 * @return The serialized line
 */
[[nodiscard]] std::string to_netscape_line(const Cookie& cookie);

/**
 * @brief Whether @p cookie may be sent to @p host — the Netscape domain rule.
 *
 * A cookie is scoped to exactly its domain unless it opted into subdomains, in
 * which case a tail match applies (a `example.com` cookie also goes to
 * `api.example.com`, but never the other way round). A leading dot on the stored
 * domain is tolerated and read as the subdomain opt-in, since that is how the
 * Netscape format spells it.
 * @param cookie The stored cookie
 * @param host Lowercased hostname of the request
 * @return true if the domain scope covers @p host
 */
[[nodiscard]] bool cookie_domain_matches(const Cookie& cookie, std::string_view host);

/**
 * @brief Whether @p cookie may be sent to @p path — the RFC 6265 path rule.
 *
 * The cookie's path must be a prefix of the request path *at a segment boundary*,
 * so a `/foo` cookie covers `/foo` and `/foo/bar` but not `/foobar`.
 * @param cookie The stored cookie
 * @param path Path of the request (a leading `/` is assumed; empty reads as `/`)
 * @return true if the path scope covers @p path
 */
[[nodiscard]] bool cookie_path_matches(const Cookie& cookie, std::string_view path);

/**
 * @brief An in-memory cookie store with Netscape persistence — the storage half a
 * @ref CookieJar needs when its backend does not provide one.
 *
 * Owns the cookies of one session and answers @ref cookies_for with the subset a
 * given request may carry. Deliberately transport-free: a backend supplies only
 * the parsing of `Set-Cookie` (whose attribute and date grammar is worth borrowing
 * from the platform) and hands the resulting cookies here.
 *
 * Requests of a parser can run concurrently, so every method is internally
 * synchronized and safe to call from any thread.
 */
class CookieStore {
public:
	/**
	 * @brief The first cookie carrying @p name, or nullopt if none is stored.
	 * @param name Cookie name to look for
	 * @return The matching cookie; the first match wins, as names are not unique
	 *         across domains and paths
	 */
	[[nodiscard]] std::optional<Cookie> find(std::string_view name) const;

	/**
	 * @brief Every cookie currently stored, expired ones included.
	 * @return A snapshot, in insertion order
	 */
	[[nodiscard]] std::vector<Cookie> all() const;

	/**
	 * @brief The cookies a request to @p host / @p path may carry, in insertion order.
	 *
	 * Applies the domain, path, secure and expiry rules together: a `secure` cookie
	 * is withheld unless @p secure_transport, and an expired one is never returned
	 * (and is dropped from the store on the way, so a jar does not grow without
	 * bound over a long session).
	 * @param host Hostname of the request; matched case-insensitively
	 * @param path Path of the request
	 * @param secure_transport Whether the request goes over HTTPS
	 * @return The cookies to send
	 */
	[[nodiscard]] std::vector<Cookie> cookies_for(std::string_view host,
	                                              std::string_view path,
	                                              bool secure_transport);

	/**
	 * @brief Store @p cookie, replacing any stored one with the same name, domain
	 * and path — the RFC 6265 identity of a cookie.
	 * @param cookie The cookie to store
	 */
	void set(const Cookie& cookie);

	/// Drop every cookie, ending the session the store was holding.
	void clear();

	/**
	 * @brief Export every stored cookie as Netscape lines. @see to_netscape_line
	 * @return One line per stored cookie
	 */
	[[nodiscard]] std::vector<std::string> serialize() const;

	/**
	 * @brief Replace the contents with a previously serialized set; unparsable lines
	 * are skipped. The store is cleared first, so what remains is exactly @p lines.
	 * @param lines Lines produced by @ref serialize
	 */
	void deserialize(std::span<std::string> lines);

private:
	mutable std::mutex mutex_;
	std::vector<Cookie> cookies_;
};

/**
 * @brief A @ref CookieJar backed by an in-memory @ref CookieStore — the jar a
 * backend mints when the transport has no per-session cookie store to hand out.
 *
 * Deliberately transport-free: it forwards the jar interface onto the store and
 * exposes the store itself, which is what the backend reads to decide the cookies
 * for a request and writes the ones a response set. So the cookie layer is
 * testable without any transport, and a jar serialized under one backend restores
 * under another (both speak @ref to_netscape_line).
 */
class MemoryCookieJar : public CookieJar {
public:
	[[nodiscard]] std::optional<Cookie> find_cookie(std::string_view name) const override;
	[[nodiscard]] std::vector<Cookie> cookies() const override;
	void set_cookie(const Cookie& cookie) override;
	void clear() override;
	[[nodiscard]] std::vector<std::string> serialize() const override;
	void deserialize(std::span<std::string> cookies) override;

	/**
	 * @brief The store behind this jar — the backend's door to the matching rules
	 * the @ref CookieJar interface does not expose.
	 * @return The store; it lives as long as the jar
	 */
	[[nodiscard]] CookieStore& store() noexcept { return store_; }

private:
	CookieStore store_;
};
} // namespace aniparse
