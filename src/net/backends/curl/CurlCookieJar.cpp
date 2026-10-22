/*
 * Copyright (C) 2026 Toilettrauma
 */
#if defined(ANIPARSE_CURL_BACKEND) && ANIPARSE_CURL_BACKEND
#include "aniparse/net/backends/curl/CurlCookieJar.hpp"
#include "aniparse/net/CookieStore.hpp"
#include <curlpp/Options.hpp>
namespace aniparse {
CurlCookieJar::CurlCookieJar()
    : shared_(std::make_shared<asyncnet::CurlShared>()) {
	shared_->set_cookies_shared(true);
}

CurlCookieJar::CurlCookieJar(std::shared_ptr<asyncnet::CurlShared> shared)
    : shared_(std::move(shared)) {
}

std::optional<Cookie> CurlCookieJar::find_cookie(std::string_view name) const {
	for (const auto& line : serialize()) {
		if (auto cookie = parse_netscape_line(line); cookie && cookie->name == name) {
			return cookie;
		}
	}
	return std::nullopt;
}

std::vector<Cookie> CurlCookieJar::cookies() const {
	std::vector<Cookie> result;
	for (const auto& line : serialize()) {
		if (auto cookie = parse_netscape_line(line)) {
			result.push_back(std::move(*cookie));
		}
	}
	return result;
}

void CurlCookieJar::set_cookie(const Cookie& cookie) {
	asyncnet::Request tmp_request;
	tmp_request.set_share(shared_);
	tmp_request.set_cookie_file(asyncnet::Request::cookie_memory);

	auto handle = tmp_request.make_request_handle();
	handle.setOpt(curlpp::options::CookieList(to_netscape_line(cookie)));
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

} // namespace aniparse
#endif // ANIPARSE_CURL_BACKEND
