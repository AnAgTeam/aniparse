/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Client.hpp"
#include "aniparse/Headers.hpp"
#include "aniparse/types/NetError.hpp"

#include <asyncnet/Exceptions.hpp>
#include <curlpp/Options.hpp>

#include <array>
#include <charconv>

using asyncnet::NetworkTask;
// NB: don't `using asyncnet::Response` here — inside namespace aniparse the
// unqualified name would resolve to aniparse::Response<T> (the expected alias),
// not the curl-bound HTTP response. Qualify asyncnet::Response explicitly.

namespace aniparse {

// Map a libcurl result code onto the backend-neutral classification. Lives here,
// in the curl backend, so the neutral NetError type stays free of curl headers.
static NetErrc classify_curl_error(CURLcode code) {
	switch (code) {
	case CURLE_OPERATION_TIMEDOUT:
		return NetErrc::timed_out;
	case CURLE_ABORTED_BY_CALLBACK:
		return NetErrc::cancelled;
	case CURLE_COULDNT_CONNECT:
	case CURLE_COULDNT_RESOLVE_HOST:
	case CURLE_COULDNT_RESOLVE_PROXY:
		return NetErrc::connect_failed;
	case CURLE_SSL_CONNECT_ERROR:
	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_SSL_CERTPROBLEM:
	case CURLE_SSL_CIPHER:
	case CURLE_SSL_CACERT_BADFILE:
		return NetErrc::tls_failed;
	default:
		return NetErrc::other;
	}
}

// Translate the curl-bound transport exception into the neutral NetError that
// escapes the client. Transport failures carry no HTTP status (there is no
// response), so http_status stays 0.
static NetError to_net_error(const asyncnet::NetworkRuntimeError& error) {
	return NetError(classify_curl_error(error.whatCode()), 0, error.what());
}

template <typename Functor>
NetworkTask<asyncnet::Response> with_retry(uint32_t max_retries,
                                           Functor operation) {
	for (uint32_t i = 0; i <= max_retries; ++i) {
		try {
			co_return co_await operation();
		} catch (const asyncnet::NetworkRuntimeError& raw_error) {
			NetError error = to_net_error(raw_error);
			// A cancelled request was stopped on purpose — never retry it.
			if (error.code == NetErrc::cancelled || i == max_retries) {
				throw error;
			}
			// TODO:
			//if (delay.count() > 0) {
			//	co_await asyncnet::sleep(delay);
			//}
		}
	}
	throw std::logic_error("with_retry: unreachable");
}

// Flatten the backend's curl-bound Response into the neutral ResponseData that
// crosses into parser code. Status is read off the lvalue before the body is
// moved out (designated initializers evaluate left to right).
static ResponseData to_response_data(asyncnet::Response&& response) {
	return ResponseData{
	    .status_code = response.get_status_code(),
	    .body        = std::move(response).get_text(),
	};
}

// Translate the neutral, editable UrlParameters into asyncnet's pre-serialized
// form. asyncnet percent-encodes each pair as it is appended, keeping URL
// encoding a single-sourced transport concern.
static asyncnet::UrlParameters to_asyncnet_params(const UrlParameters& params) {
	asyncnet::UrlParameters result;
	for (const auto& [key, value] : params) {
		result += std::pair<std::string_view, std::string_view>(key, value);
	}
	return result;
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

namespace {

// Netscape-format cookie line (as produced by CURLINFO_COOKIELIST):
//   domain \t include_subdomains \t path \t secure \t expires \t name \t value
// httponly cookies carry a "#HttpOnly_" prefix on the domain field. The value is
// the trailing field and is taken verbatim (tabs and all).
constexpr std::string_view httponly_prefix = "#HttpOnly_";

std::optional<Cookie> parse_netscape_line(std::string_view line) {
	constexpr size_t field_count = 7;

	std::array<std::string_view, field_count> fields;
	size_t count = 0;
	size_t start = 0;

	while (count < field_count) {
		// Last field (the value) is the remainder of the line, tabs and all.
		size_t tab = (count == field_count - 1) ? std::string_view::npos : line.find('\t', start);
		if (tab == std::string_view::npos) {
			fields[count++] = line.substr(start);
			break;
		}
		fields[count++] = line.substr(start, tab - start);
		start = tab + 1;
	}

	if (count != field_count) {
		return std::nullopt;
	}

	Cookie cookie;
	std::string_view domain = fields[0];
	if (domain.starts_with(httponly_prefix)) {
		cookie.http_only = true;
		domain.remove_prefix(httponly_prefix.size());
	}
	cookie.domain             = std::string(domain);
	cookie.include_subdomains = fields[1] == "TRUE";
	cookie.path               = std::string(fields[2]);
	cookie.secure             = fields[3] == "TRUE";

	long long expires_unix = 0;
	std::from_chars(fields[4].data(), fields[4].data() + fields[4].size(), expires_unix);
	if (expires_unix != 0) {
		cookie.expires = std::chrono::system_clock::time_point(std::chrono::seconds(expires_unix));
	}

	cookie.name  = std::string(fields[5]);
	cookie.value = std::string(fields[6]);
	return cookie;
}

std::string to_netscape_line(const Cookie& cookie) {
	long long expires_unix = cookie.expires
	    ? std::chrono::duration_cast<std::chrono::seconds>(cookie.expires->time_since_epoch()).count()
	    : 0;

	std::string line;
	if (cookie.http_only) {
		line += httponly_prefix;
	}
	line += cookie.domain;
	line += '\t';
	line += cookie.include_subdomains ? "TRUE" : "FALSE";
	line += '\t';
	line += cookie.path;
	line += '\t';
	line += cookie.secure ? "TRUE" : "FALSE";
	line += '\t';
	line += std::to_string(expires_unix);
	line += '\t';
	line += cookie.name;
	line += '\t';
	line += cookie.value;
	return line;
}

} // namespace

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

NetworkTask<ResponseData> AsyncClient::do_request(ConfiguredGetRequest configured_request) {
	auto session  = session_.load();
	auto& request = configured_request.request;

	auto get_request = session->make_request<asyncnet::GetRequest>(std::move(request.url));
	get_request.add_headers(to_header_lines(request.headers));
	get_request.set_url_parameters(to_asyncnet_params(request.url_params));
	apply_cookie_share(get_request, configured_request.cookies);

	// Okay to hold references (ref to frame variable), because we will wait for next coroutine end
	co_return to_response_data(co_await with_retry(max_retries_, [&get_request, &session]() {
		return session->perform_request(get_request);
	}));
}

NetworkTask<ResponseData> AsyncClient::do_request(ConfiguredPostRequest configured_request) {
	auto session  = session_.load();
	auto& request = configured_request.request;

	auto post_request = session->make_request<asyncnet::PostRequest>(std::move(request.url), std::move(request.body));
	post_request.add_headers(to_header_lines(request.headers));
	post_request.set_url_parameters(to_asyncnet_params(request.url_params));
	apply_cookie_share(post_request, configured_request.cookies);

	// Okay to hold references (ref to frame variable), because we will wait for next coroutine end
	co_return to_response_data(co_await with_retry(max_retries_, [&post_request, &session]() {
		return session->perform_request(post_request);
	}));
}

NetworkTask<ResponseData> AsyncClient::do_request(ConfiguredPostMultipartRequest) {
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