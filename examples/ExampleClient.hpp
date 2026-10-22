#pragma once
#include "aniparse/runtime/RequestContext.hpp"
#include "aniparse/net/backends/curl/CurlHttpClient.hpp"

namespace aniparse::examples {

#ifndef ANIPARSE_CURL_BACKEND
// Offline examples need a context for parsing/caching, but make no HTTP calls.
class OfflineClient final : public HttpClient {
public:
	NetworkRequestTask<HttpResponse> do_request(ConfiguredGetRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
	}
	NetworkRequestTask<HttpResponse> do_request(ConfiguredPostRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
	}
	NetworkRequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
	}
	void apply_client_config(HttpClientConfig) override {}
	std::shared_ptr<CookieJar> make_cookie_jar() override { return nullptr; }
};
#endif

inline std::shared_ptr<HttpClient> make_client() {
#ifdef ANIPARSE_CURL_BACKEND
	return std::make_shared<CurlHttpClient>();
#else
	return std::make_shared<OfflineClient>();
#endif
}

} // namespace aniparse::examples
