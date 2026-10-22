/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/HttpClientConfig.hpp"
#include "aniparse/net/Request.hpp"
#include "aniparse/net/Response.hpp"
#include "aniparse/utility/Result.hpp"
#include "aniparse/net/CookieJar.hpp"
#include "aniparse/utility/AtomicSharedPtr.hpp"
#include <memory>

namespace aniparse {

/**
 * @brief Interface for performing HTTP requests — the backend-neutral seam the whole
 * parse core sits on.
 *
 * Parsers and getters never touch an HTTP library: they talk to a @ref RequestContext,
 * which talks to this interface and nothing else. So the transport is swappable — the
 * implementing side is a backend (the bundled curl-based one, or a platform client
 * supplied by the host application), and the consuming side is the parse core. A
 * backend implements every method here; parser code implements none of them and calls
 * none of them directly.
 *
 * An implementation is shared (through a @ref RuntimeServices) by every context and every
 * parser of a session, so it must tolerate concurrent requests.
 */
struct HttpClient {
	virtual ~HttpClient() = default;

	/**
	 * @brief Perform HTTP GET request
	 * @param request HTTP request
	 * @return Task with the response, or a RequestError on a transport-level failure
	 *         (timeout, connection, TLS, cancellation). No status check here — the
	 *         raw HTTP status stays in HttpResponse for the caller to inspect.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<HttpResponse> do_request(ConfiguredGetRequest request) = 0;

	/**
	 * @brief Perform HTTP POST request
	 * @param request HTTP request
	 * @return Task with the response, or a RequestError on a transport-level failure
	 *         (timeout, connection, TLS, cancellation). No status check here.
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<HttpResponse> do_request(ConfiguredPostRequest request) = 0;

	/**
	 * @brief Perform HTTP POST multipart/form-data request
	 * @param request HTTP request
	 * @return Task with the response, or a RequestError on a transport-level failure.
	 *         No status check here.
	 * @throws std::invalid_argument If a File form part sets an explicit filename
	 *         (the transport presents the on-disk name and cannot override it) — a caller bug,
	 *         raised when the returned task is awaited
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	virtual RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest request) = 0;

	/**
	 * @brief Adopt a new transport-wide configuration.
	 *
	 * Affects requests started after the call; requests already in flight keep the
	 * configuration they were started with. Not a per-parser knob — every parser
	 * sharing this client sees the change. @see ParserConfig for per-parser settings.
	 * @param config The transport-wide configuration to apply
	 */
	void set_config(HttpClientConfig config);

    /**
     * @brief Read the currently published application request defaults.
     * @return An owned snapshot, unaffected by subsequent configuration updates.
     */
    RequestDefaults request_defaults() const;

    /**
     * @brief Apply transport settings for a configuration update.
     * @param config The new client configuration; common defaults are handled by the base.
     * @note Backends implement this hook instead of overriding set_config.
     */
    virtual void apply_client_config(HttpClientConfig config) = 0;

	/**
	 * @brief Create an empty cookie jar this client can drive.
	 *
	 * The jar is backend-specific (it wraps whatever cookie store the transport
	 * actually reads), which is why it is minted here rather than constructed by the
	 * caller. A context provisions a jar this way for a config that has none, so a
	 * parser gets its own session automatically.
	 * @return A fresh empty jar, or nullptr for a backend that manages cookies itself
	 */
	virtual std::shared_ptr<CookieJar> make_cookie_jar() = 0;
protected:
	// Atomic publication state makes transport clients noncopyable and nonmovable.
	HttpClient() = default;
	HttpClient(const HttpClient&) = delete;
	HttpClient(HttpClient&&) = delete;
	HttpClient& operator=(const HttpClient&) = delete;
	HttpClient& operator=(HttpClient&&) = delete;

private:
    AtomicSharedPtr<const RequestDefaults> request_defaults_;

};

} // namespace aniparse
