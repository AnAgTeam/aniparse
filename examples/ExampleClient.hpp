#pragma once
#include "aniparse/source/Parser.hpp"
#include "aniparse/net/backends/curl/CurlHttpClient.hpp"

namespace aniparse::examples {

#ifndef ANIPARSE_CURL_BACKEND
// Offline examples need a context for parsing/caching, but make no HTTP calls.
class OfflineClient final : public HttpClient {
public:
	RequestTask<HttpResponse> do_request(ConfiguredGetRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "No HTTP backend selected");
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

inline std::shared_ptr<RuntimeServices> make_services() {
    auto services = std::make_shared<RuntimeServices>();
    services->client = make_client();
    return services;
}

// Parsing-only tutorials still bind sessions through the ordinary parser factory.
inline std::unique_ptr<ParserProfile> make_profile() {
    struct ExampleParser final : Parser {
        ParserInfo info() const override { return {.name = "Example"}; }
        std::string identifier() const override { return "Example"; }
        void emplace_domains(EmplaceDomainsSink&) const override {}
    } parser;
    return parser.make_profile(default_profile_id);
}

} // namespace aniparse::examples
