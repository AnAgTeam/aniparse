/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Client.hpp"
#include "aniparse/Headers.hpp"

#include <asyncnet/Exceptions.hpp>
#include <curlpp/Options.hpp>

#include <array>

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

// Route the request onto the cookie jar's own share handle so that every
// request type (GET/POST/multipart) reads and writes the same cookie store.
// Without this a POST login would land in the session-default store while GET
// reads from the per-config jar, and two parser instances would collide.
static void apply_cookie_share(asyncnet::Request& request, const std::shared_ptr<CookieJar>& jar) {
	if (auto curl_jar = std::dynamic_pointer_cast<CurlCookieJar>(jar)) {
		request.set_share(curl_jar->shared());
	} else {
		request.set_share(nullptr);
	}
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

std::optional<std::string> CurlCookieJar::find_cookie(std::string_view name) const {
	// serialize() yields Netscape-format lines from CURLINFO_COOKIELIST:
	//   domain \t include_subdomains \t path \t secure \t expires \t name \t value
	// (httponly cookies carry a "#HttpOnly_" prefix on the domain field, which we
	// ignore). The value is the trailing field and is taken verbatim.
	constexpr size_t field_count = 7;
	constexpr size_t name_field  = 5;
	constexpr size_t value_field = 6;

	for (const auto& line : serialize()) {
		std::array<std::string_view, field_count> fields;
		std::string_view view = line;
		size_t count = 0;
		size_t start = 0;

		while (count < field_count) {
			// Last field (the value) is the remainder of the line, tabs and all.
			size_t tab = (count == field_count - 1) ? std::string_view::npos : view.find('\t', start);
			if (tab == std::string_view::npos) {
				fields[count++] = view.substr(start);
				break;
			}
			fields[count++] = view.substr(start, tab - start);
			start = tab + 1;
		}

		if (count == field_count && fields[name_field] == name) {
			return std::string(fields[value_field]);
		}
	}

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
	apply_cookie_share(get_request, configured_request.cookies);

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
	apply_cookie_share(post_request, configured_request.cookies);

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