/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/net/Request.hpp"
#include "aniparse/net/HttpClient.hpp"

// The curl HTTP backend. Only compiled with ANIPARSE_CURL_BACKEND; the whole
// header collapses to nothing otherwise, so a stray include never pulls asyncnet
// (hence curl) into a backend-less or NSURLSession-only build.
#if defined(ANIPARSE_CURL_BACKEND) && ANIPARSE_CURL_BACKEND

#include <asyncnet/AsyncSession.hpp>
#include <asyncnet/Requestor.hpp>
#include <atomic>

#include "aniparse/utility/AtomicSharedPtr.hpp"
#include "aniparse/net/backends/curl/CurlCookieJar.hpp"

namespace aniparse {

/**
 * @brief Class for performing asyncronous HTTP requests
 * (C++20 coroutine based)
 * Can be used as client for parsers. @see RequestContext
 */
class CurlHttpClient : public HttpClient {
public:
	static constexpr size_t default_max_retries = 4;

	/**
	 * @brief Initializeclient
	 */
	CurlHttpClient();

	/**
	 * @brief Perform HTTP GET request
	 * @param request HTTP request
	 * @return Task with response
	 */
	RequestTask<HttpResponse> do_request(ConfiguredGetRequest request) override;

	/**
	 * @brief Perform HTTP POST request
	 * @param request HTTP request
	 * @return Task with response
	 */
	RequestTask<HttpResponse> do_request(ConfiguredPostRequest request) override;

	/**
	 * @brief Perform HTTP POST multipart/form-data request
	 * @param request HTTP request
	 * @return Task with response
	 */
	RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest request) override;

	HttpClientConfig config() const;

	void apply_client_config(HttpClientConfig new_config) override;

	std::shared_ptr<CookieJar> make_cookie_jar() override;

private:
	std::shared_ptr<asyncnet::Requestor> core_;
	AtomicSharedPtr<asyncnet::AsyncSession> session_;

	// Independent retry budget: concurrent requests may observe the old or new value.
	// It does not publish other state, so relaxed loads and stores are sufficient.
	std::atomic<uint32_t> max_retries_;
};
} // namespace aniparse

#endif // ANIPARSE_CURL_BACKEND
