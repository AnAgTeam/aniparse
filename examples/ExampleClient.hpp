#pragma once
#include "aniparse/ClientContext.hpp"
#include "aniparse/net/Client.hpp"

namespace aniparse::examples {

#ifndef ANIPARSE_CURL_BACKEND
// Offline examples need a context for parsing/caching, but make no HTTP calls.
class OfflineClient final : public ClientContext {
public:
	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
	}
	void apply_client_config(ClientConfig) override {}
	std::shared_ptr<CookieJar> make_cookie_jar() override { return nullptr; }
};
#endif

inline std::shared_ptr<ClientContext> make_client() {
#ifdef ANIPARSE_CURL_BACKEND
	return std::make_shared<AsyncClient>();
#else
	return std::make_shared<OfflineClient>();
#endif
}

} // namespace aniparse::examples
