/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Requests.hpp"
#include "aniparse/ClientContext.hpp"

#include <asyncnet/AsyncSession.hpp>
#include <asyncnet/Requestor.hpp>

namespace aniparse {

class CurlCookieJar : public CookieJar {
public:
	CurlCookieJar();
	CurlCookieJar(std::shared_ptr<asyncnet::CurlShared> shared);

	// @todo
	std::optional<std::string> find_cookie(std::string_view name) const override;

	// @todo
	void set_cookie(std::string cookie) override;

	// @todo
	void clear() override;

	std::vector<std::string> serialize() const override;
	void deserialize(std::span<std::string> cookies) override;

	const std::shared_ptr<asyncnet::CurlShared>& shared() const;

private:
	std::shared_ptr<asyncnet::CurlShared> shared_;
};

/**
 * @brief Class for performing asyncronous HTTP requests
 * (C++20 coroutine based)
 * Can be used as client for parsers. @see RequestorContext
 */
class AsyncClient : public ClientContext {
public:
	static constexpr size_t default_max_retries = 4;

	/**
	 * @bief Initialize client
	 */
	AsyncClient();

	/**
	 * @bief Perform HTTP GET request
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<asyncnet::Response> do_request(ConfiguredGetRequest request) override;

	/**
	 * @bief Perform HTTP POST request
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<asyncnet::Response> do_request(ConfiguredPostRequest request) override;

	/**
	 * @bief Perform HTTP POST multipart/form-data request
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<asyncnet::Response> do_request(ConfiguredPostMultipartRequest request) override;

	ClientConfig config() const;

	void set_config(ClientConfig new_config) override;

	std::shared_ptr<CookieJar> make_cookie_jar() override;

private:
	std::shared_ptr<asyncnet::Requestor> core_;
	std::atomic<std::shared_ptr<asyncnet::AsyncSession>> session_;

	uint32_t max_retries_;
};
} // namespace aniparse