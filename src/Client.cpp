/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Client.hpp"
#include "aniparse/Headers.hpp"

#include <asyncnet/Exceptions.hpp>
#include <curlpp/Options.hpp>

using asyncnet::NetworkTask;
using asyncnet::Response;

namespace aniparse {

template <typename Functor>
NetworkTask<Response> with_retry(uint32_t max_retries,
                                 Functor operation) {
	for (uint32_t i = 0; i <= max_retries; ++i) {
		try {
			co_return co_await operation();
		} catch (const asyncnet::NetworkRuntimeError& e) {
			if (i == max_retries) {
				std::rethrow_exception(std::current_exception());
			}
			// TODO:
			//if (delay.count() > 0) {
			//	co_await asyncnet::sleep(delay);
			//}
		}
	}
	throw std::logic_error("with_retry: unreachable");
}

AsyncClient::AsyncClient()
    : core_(asyncnet::Requestor::make_shared()), session_(std::make_shared<asyncnet::AsyncSession>(core_)), max_retries_(default_max_retries) {
}

CurlCookieJar::CurlCookieJar()
    : shared_(std::make_shared<asyncnet::CurlShared>()) {
	shared_->set_cookies_shared(true);
}

CurlCookieJar::CurlCookieJar(std::shared_ptr<asyncnet::CurlShared> shared)
    : shared_(std::move(shared)) {
}

// @todo
std::optional<std::string> CurlCookieJar::find_cookie(std::string_view name) const {
	return std::nullopt;
}

// @todo
void CurlCookieJar::set_cookie(std::string cookie) {
}

void CurlCookieJar::clear() {
	asyncnet::Request tmp_request;
	tmp_request.set_share(shared_);
	tmp_request.set_cookie_file(asyncnet::Request::cookie_memory);

	auto handle = tmp_request.make_request_handle();

	// clear all
	handle.setOpt(curlpp::options::CookieList("ALL"));
}

std::vector<std::string> CurlCookieJar::serialize() const {
	asyncnet::Request tmp_request;
	tmp_request.set_share(shared_);
	tmp_request.set_cookie_file(asyncnet::Request::cookie_memory);

	auto handle = tmp_request.make_request_handle();

	curl_slist* cookies_list_raw = nullptr;
	handle.getCurlHandle().getInfo(CURLINFO_COOKIELIST, cookies_list_raw);
	if (!cookies_list_raw) {
		return {};
	}

	// RAII: destroy if errored
	auto cookies_list = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>(cookies_list_raw,
	                                                                                curl_slist_free_all);

	std::vector<std::string> serialized_cookies;
	for (auto* cookie_iter = cookies_list.get(); cookie_iter; cookie_iter = cookie_iter->next) {
		serialized_cookies.emplace_back(cookie_iter->data);
	}

	return serialized_cookies;
}

void CurlCookieJar::deserialize(std::span<std::string> cookies) {
	asyncnet::Request tmp_request;
	tmp_request.set_share(shared_);
	tmp_request.set_cookie_file(asyncnet::Request::cookie_memory);

	auto handle = tmp_request.make_request_handle();

	// clear all
	handle.setOpt(curlpp::options::CookieList("ALL"));

	for (auto cookie : cookies) {
		handle.setOpt(curlpp::options::CookieList(cookie));
	}
}

const std::shared_ptr<asyncnet::CurlShared>& CurlCookieJar::shared() const {
	return shared_;
}

NetworkTask<Response> AsyncClient::do_request(ConfiguredGetRequest configured_request) {
	auto session  = session_.load();
	auto& request = configured_request.request;

	auto get_request = session->make_request<asyncnet::GetRequest>(std::move(request.url));
	get_request.add_headers(to_header_lines(request.headers));
	get_request.set_url_parameters(std::move(request.url_params));
	if (auto cookies = std::dynamic_pointer_cast<CurlCookieJar>(configured_request.cookies)) {
		get_request.set_share(cookies->shared());
	} else {
		get_request.set_share(nullptr);
	}

	// Okay to hold references (ref to frame variable), because we will wait for next coroutine end
	co_return co_await with_retry(max_retries_, [&get_request, &session]() {
		return session->perform_request(get_request);
	});
}

NetworkTask<Response> AsyncClient::do_request(ConfiguredPostRequest configured_request) {
	auto session  = session_.load();
	auto& request = configured_request.request;

	auto post_request = session->make_request<asyncnet::PostRequest>(std::move(request.url), std::move(request.body));
	post_request.add_headers(to_header_lines(request.headers));
	post_request.set_url_parameters(std::move(request.url_params));

	// Okay to hold references (ref to frame variable), because we will wait for next coroutine end
	co_return co_await with_retry(max_retries_, [&post_request, &session]() {
		return session->perform_request(post_request);
	});
}

NetworkTask<Response> AsyncClient::do_request(ConfiguredPostMultipartRequest) {
	throw std::logic_error("Not implemented");
}

// TODO: maybe make max_retries atomic (or who even cares?)
// Worst case: request uses old retry count. Not a big deal.
void AsyncClient::set_config(ClientConfig new_config) {
	auto new_session = std::make_shared<asyncnet::AsyncSession>(core_);
	new_session->set_timeout(new_config.timeout);

	// raw
	if (new_config.proxy) {
		new_session->set_option<curlpp::options::Proxy>(new_config.proxy->host);
		new_session->set_option<curlpp::options::ProxyPort>(new_config.proxy->port);

		std::string user_passwd = new_config.proxy->user + ":" + new_config.proxy->passwd;
		new_session->set_option<curlpp::options::ProxyUserPwd>(user_passwd);
	}

	new_session->set_option<curlpp::options::SslVerifyHost>(new_config.flags.test(client_config_flags::verify_ssl));
	new_session->set_option<curlpp::options::SslVerifyPeer>(new_config.flags.test(client_config_flags::verify_ssl));

	session_.store(new_session);
	max_retries_ = new_config.max_retries.value_or(default_max_retries);
}

std::shared_ptr<CookieJar> AsyncClient::make_cookie_jar() {
	return std::make_shared<CurlCookieJar>();
}

} // namespace aniparse