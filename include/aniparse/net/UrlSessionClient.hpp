/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/ClientContext.hpp"
#include "aniparse/types/Request.hpp"

// The NSURLSession HTTP backend (Apple). Only compiled with
// ANIPARSE_NSURLSESSION_BACKEND; the whole header collapses to nothing otherwise,
// so a stray include never drags Foundation into a portable build.
#if defined(ANIPARSE_NSURLSESSION_BACKEND) && ANIPARSE_NSURLSESSION_BACKEND

#include <memory>

namespace aniparse {

/**
 * @brief The Apple HTTP backend: a @ref ClientContext performing requests through
 * NSURLSession, so the platform owns TLS, proxies, and the connection pool.
 *
 * Interchangeable with the curl backend by construction — it is chosen where the
 * client is built and nothing above it can tell the difference. Two behaviours are
 * matched to the curl backend deliberately rather than taken from Foundation's
 * defaults, since a parser cannot be written against a transport that differs per
 * platform:
 *  - **Redirects are not followed.** A 3xx is surfaced as the response, so a parser
 *    can read its Location. NSURLSession would otherwise follow automatically.
 *  - **Cookies are not handled by the session.** The session's own store is one per
 *    session, which cannot express the per-parser jars this library hands out, so
 *    the jar (@ref MemoryCookieJar) owns them and this client attaches and harvests
 *    them per request.
 *
 * Where the contract leaves a default to the backend, Foundation's is kept — most
 * visibly the request timeout, which is 60s here rather than curl's "no timeout"
 * (@see ClientConfig::timeout).
 *
 * Shared by every context of a session, so it performs requests concurrently.
 */
class UrlSessionClient : public ClientContext {
public:
	/// Transport-failure retries when @ref ClientConfig::max_retries is unset —
	/// matched to the curl backend so a swap does not change how stubborn a request is.
	static constexpr uint32_t default_max_retries = 4;

	UrlSessionClient();
	~UrlSessionClient() override;

	UrlSessionClient(const UrlSessionClient&)            = delete;
	UrlSessionClient& operator=(const UrlSessionClient&) = delete;

	/**
	 * @brief Perform HTTP GET request
	 * @param request HTTP request
	 * @return Task with the response, or a RequestError on a transport-level failure
	 */
	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest request) override;

	/**
	 * @brief Perform HTTP POST request
	 * @param request HTTP request
	 * @return Task with the response, or a RequestError on a transport-level failure
	 */
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest request) override;

	/**
	 * @brief Perform HTTP POST multipart/form-data request
	 * @param request HTTP request
	 * @return Task with the response, or a RequestError on a transport-level failure
	 * @throws std::invalid_argument If a File form part cannot be read at send time
	 */
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest request) override;

	/**
	 * @brief Adopt a new transport-wide configuration.
	 *
	 * An NSURLSessionConfiguration is copied into its session and immutable after, so
	 * this builds a fresh session: requests started after the call use it, while ones
	 * already in flight keep the session (and thus the configuration) they started on,
	 * which is exactly the contract.
	 * @param config The transport-wide configuration to apply
	 */
	void set_config(ClientConfig config) override;

	/**
	 * @brief Mint an empty cookie jar for a parser's session.
	 * @return A fresh @ref MemoryCookieJar — this backend keeps cookies out of the
	 *         session, so the jar owns them. @see UrlSessionClient
	 */
	std::shared_ptr<CookieJar> make_cookie_jar() override;

private:
	// Hides the Objective-C session behind a plain C++ type, so this header stays
	// includable from ordinary C++ translation units.
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace aniparse

#endif // ANIPARSE_NSURLSESSION_BACKEND
