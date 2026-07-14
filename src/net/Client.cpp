/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
// The curl HTTP backend implementation. Compiled only when the curl backend is
// enabled (CMake also drops it from the source list then); this guard makes an
// accidental build a clean no-op rather than a curlpp-not-found error.
#if defined(ANIPARSE_CURL_BACKEND) && ANIPARSE_CURL_BACKEND

#include "aniparse/net/Client.hpp"
#include "aniparse/types/Headers.hpp"
#include "aniparse/utility/UrlEncode.hpp"

#include <asyncnet/Exceptions.hpp>
#include <asyncnet/MultipartForms.hpp>
#include <curlpp/Options.hpp>

#include <array>
#include <charconv>

using asyncnet::NetworkTask;
// NB: don't `using asyncnet::Response` here — inside namespace aniparse the
// unqualified name would resolve to aniparse::Response<T> (the expected alias),
// not the curl-bound HTTP response. Qualify asyncnet::Response explicitly.

namespace aniparse {

// Map a libcurl transport error onto a coarse RequestErrorCode. Cancellation is
// deliberate (stop requested); everything else collapses to one NetworkError —
// the exact curl code carries nothing the consumer branches on.
static RequestErrorCode curl_error_to_code(CURLcode code) {
	return code == asyncnet::CancelledErrorCode ? RequestErrorCode::Cancelled
	                                            : RequestErrorCode::NetworkError;
}

// Retries the operation on a transport error until the policy is exhausted, then
// rethrows the raw curl exception for do_request to fold into a RequestError. A
// cancelled request was stopped on purpose, so it is never retried.
template <typename Functor>
NetworkTask<asyncnet::Response> with_retry(uint32_t max_retries,
                                           Functor operation) {
	for (uint32_t i = 0; i <= max_retries; ++i) {
		try {
			co_return co_await operation();
		} catch (const asyncnet::NetworkRuntimeError& error) {
			if (error.whatCode() == asyncnet::CancelledErrorCode || i == max_retries) {
				throw;
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
// crosses into parser code. Every field is read off the lvalue first; the body
// is moved out last so no accessor runs on a moved-from response.
static ResponseData to_response_data(asyncnet::Response&& response) {
	long status               = response.get_status_code();
	Headers headers           = parse_header_block(response.get_header_block());
	std::string effective_url = response.get_effective_url();
	std::string body          = std::move(response).get_text();

	ResponseData data{
	    .status_code = status,
	    .body        = std::move(body),
	    .headers     = std::move(headers),
	};
	if (!effective_url.empty()) {
		data.effective_url = std::move(effective_url);
	}
	return data;
}

// Apply the request verb onto an already-built asyncnet request. The request type
// fixes the body shape and a natural default verb (GET/POST); this overrides the
// verb when the caller chose another. GET/POST need no override (curl warns against
// CUSTOMREQUEST for them); HEAD uses NOBODY (the curl-blessed way); the rest go
// through CUSTOMREQUEST, reusing whatever body the request type set. An unhandled
// verb yields NotImplemented so the backend never silently downgrades to GET/POST.
static std::optional<RequestErrorCode> apply_method(asyncnet::Request& request, HttpMethod method) {
	switch (method) {
	case HttpMethod::Get:
	case HttpMethod::Post:
		return std::nullopt;
	case HttpMethod::Head:
		request.set_option<curlpp::options::NoBody>(true);
		return std::nullopt;
	case HttpMethod::Put:
		request.set_option<curlpp::options::CustomRequest>("PUT");
		return std::nullopt;
	case HttpMethod::Patch:
		request.set_option<curlpp::options::CustomRequest>("PATCH");
		return std::nullopt;
	case HttpMethod::Delete:
		request.set_option<curlpp::options::CustomRequest>("DELETE");
		return std::nullopt;
	case HttpMethod::Options:
		request.set_option<curlpp::options::CustomRequest>("OPTIONS");
		return std::nullopt;
	}
	return RequestErrorCode::NotImplemented;
}

// Translate the neutral, editable UrlParameters into asyncnet's pre-serialized
// form. asyncnet percent-encodes each pair as it is appended, keeping URL
// encoding a single-sourced transport concern.
static asyncnet::UrlParameters to_asyncnet_params(const UrlParameters& params) {
	// The neutral UrlParameters holds raw (unencoded) pairs by contract; the
	// transport percent-encodes them on send (see UrlEncode.hpp). asyncnet only
	// concatenates key=value&..., so a value with a space/':'/'&' would otherwise
	// produce a malformed URL and the request fails at the curl layer.
	asyncnet::UrlParameters result;
	for (const auto& [key, value] : params) {
		// The encoded strings must outlive the operator+= call: append_items copies
		// their bytes into the target immediately, so the views never escape this
		// iteration. Keep them as named locals (not inline temporaries) so that
		// stays true if this loop is ever restructured.
		const std::string encoded_key   = url_encode(key);
		const std::string encoded_value = url_encode(value);
		result += std::pair<std::string_view, std::string_view>(encoded_key, encoded_value);
	}
	return result;
}

// Translate the neutral multipart form into asyncnet's curlpp form list:
//   Text   -> Content        (an inline field)
//   Buffer -> FileBufferPart (an in-memory file part; bytes are moved in)
//   File   -> File           (curl reads the path lazily off disk)
// curlpp's File cannot override the presented filename, so a File part carrying
// an explicit filename is rejected — use a Buffer if a custom name is needed.
// Takes the form by value so the Buffer bytes can be moved rather than copied.
static asyncnet::MultipartForms to_curlpp_forms(MultipartForm form) {
	asyncnet::MultipartForms result;
	for (auto& part : form) {
		if (auto* text = std::get_if<MultipartPart::Text>(&part.source)) {
			if (part.content_type) {
				result.emplace_back(new asyncnet::MultipartContentPart(part.name, text->value, *part.content_type));
			} else {
				result.emplace_back(new asyncnet::MultipartContentPart(part.name, text->value));
			}
		} else if (auto* buffer = std::get_if<MultipartPart::Buffer>(&part.source)) {
			std::string filename = part.filename.value_or(std::string{});
			if (part.content_type) {
				result.emplace_back(new asyncnet::FileBufferPart(part.name, std::move(buffer->data), std::move(filename), *part.content_type));
			} else {
				result.emplace_back(new asyncnet::FileBufferPart(part.name, std::move(buffer->data), std::move(filename)));
			}
		} else if (auto* file = std::get_if<MultipartPart::File>(&part.source)) {
			if (part.filename) {
				throw std::invalid_argument(
				    "multipart File part cannot override the presented filename via curl; use a Buffer part for a custom name");
			}
			if (part.content_type) {
				result.emplace_back(new asyncnet::MultipartFilePart(part.name, file->path, *part.content_type));
			} else {
				result.emplace_back(new asyncnet::MultipartFilePart(part.name, file->path));
			}
		}
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

NetworkRequestTask<ResponseData> AsyncClient::do_request(ConfiguredGetRequest configured_request) {
	auto session  = session_.load();
	auto& request = configured_request.request;

	auto get_request = session->make_request<asyncnet::GetRequest>(std::move(request.url));
	get_request.add_headers(to_header_lines(request.headers));
	get_request.set_url_parameters(to_asyncnet_params(request.url_params));
	apply_cookie_share(get_request, configured_request.cookies);
	if (auto method_error = apply_method(get_request, request.method)) {
		co_return make_response_error(*method_error, "HTTP method not supported by this client");
	}

	// Okay to hold references (ref to frame variable), because we will wait for next coroutine end
	try {
		asyncnet::Response response = co_await with_retry(max_retries_, [&get_request, &session]() {
			return session->perform_request(get_request);
		});
		co_return to_response_data(std::move(response));
	} catch (const asyncnet::NetworkRuntimeError& error) {
		co_return make_response_error(curl_error_to_code(error.whatCode()), error.what());
	}
}

NetworkRequestTask<ResponseData> AsyncClient::do_request(ConfiguredPostRequest configured_request) {
	auto session  = session_.load();
	auto& request = configured_request.request;

	auto post_request = session->make_request<asyncnet::PostRequest>(std::move(request.url), std::move(request.body));
	post_request.add_headers(to_header_lines(request.headers));
	post_request.set_url_parameters(to_asyncnet_params(request.url_params));
	apply_cookie_share(post_request, configured_request.cookies);
	if (auto method_error = apply_method(post_request, request.method)) {
		co_return make_response_error(*method_error, "HTTP method not supported by this client");
	}

	// Okay to hold references (ref to frame variable), because we will wait for next coroutine end
	try {
		asyncnet::Response response = co_await with_retry(max_retries_, [&post_request, &session]() {
			return session->perform_request(post_request);
		});
		co_return to_response_data(std::move(response));
	} catch (const asyncnet::NetworkRuntimeError& error) {
		co_return make_response_error(curl_error_to_code(error.whatCode()), error.what());
	}
}

NetworkRequestTask<ResponseData> AsyncClient::do_request(ConfiguredPostMultipartRequest configured_request) {
	auto session  = session_.load();
	auto& request = configured_request.request;

	// to_curlpp_forms may throw std::invalid_argument on a malformed form part
	// (a File with an explicit filename); that is a caller bug, so let it propagate
	// rather than folding it into the RequestError channel.
	auto multipart_request = session->make_request<asyncnet::PostMultipartRequest>(
	    std::move(request.url), to_curlpp_forms(std::move(request.forms)));
	multipart_request.add_headers(to_header_lines(request.headers));
	multipart_request.set_url_parameters(to_asyncnet_params(request.url_params));
	apply_cookie_share(multipart_request, configured_request.cookies);
	if (auto method_error = apply_method(multipart_request, request.method)) {
		co_return make_response_error(*method_error, "HTTP method not supported by this client");
	}

	// Okay to hold references (ref to frame variable), because we will wait for next coroutine end
	try {
		asyncnet::Response response = co_await with_retry(max_retries_, [&multipart_request, &session]() {
			return session->perform_request(multipart_request);
		});
		co_return to_response_data(std::move(response));
	} catch (const asyncnet::NetworkRuntimeError& error) {
		co_return make_response_error(curl_error_to_code(error.whatCode()), error.what());
	}
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

	new_session->set_option<curlpp::options::SslVerifyHost>(new_config.flags.has(client_config_flags::verify_ssl));
	new_session->set_option<curlpp::options::SslVerifyPeer>(new_config.flags.has(client_config_flags::verify_ssl));

	session_.store(new_session);
	max_retries_ = new_config.max_retries.value_or(default_max_retries);
}

std::shared_ptr<CookieJar> AsyncClient::make_cookie_jar() {
	return std::make_shared<CurlCookieJar>();
}

} // namespace aniparse

#endif // ANIPARSE_CURL_BACKEND