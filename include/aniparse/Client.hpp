/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Request.hpp"
#include "aniparse/ClientContext.hpp"

#include <asyncnet/AsyncSession.hpp>
#include <asyncnet/Requestor.hpp>

namespace aniparse {

class CurlCookieJar : public CookieJar {
public:
	CurlCookieJar();
	CurlCookieJar(std::shared_ptr<asyncnet::CurlShared> shared);

	[[nodiscard]] std::optional<Cookie> find_cookie(std::string_view name) const override;

	[[nodiscard]] std::vector<Cookie> cookies() const override;

	void set_cookie(const Cookie& cookie) override;

	void clear() override;

	[[nodiscard]] std::vector<std::string> serialize() const override;
	void deserialize(std::span<std::string> cookies) override;

	[[nodiscard]] const std::shared_ptr<asyncnet::CurlShared>& shared() const;

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
	 * @brief Initializeclient
	 */
	AsyncClient();

	/**
	 * @brief Perform HTTP GET request
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<ResponseData> do_request(ConfiguredGetRequest request) override;

	/**
	 * @brief Perform HTTP POST request
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<ResponseData> do_request(ConfiguredPostRequest request) override;

	/**
	 * @brief Perform HTTP POST multipart/form-data request
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<ResponseData> do_request(ConfiguredPostMultipartRequest request) override;

	ClientConfig config() const;

	void set_config(ClientConfig new_config) override;

	std::shared_ptr<CookieJar> make_cookie_jar() override;

private:
	std::shared_ptr<asyncnet::Requestor> core_;
	std::atomic<std::shared_ptr<asyncnet::AsyncSession>> session_;

	uint32_t max_retries_;
};
} // namespace aniparse