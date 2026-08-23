/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/ClientContext.hpp"
#include "aniparse/cache/ResourceCache.hpp"
#include "aniparse/html/HTMLParser.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <cassert>
#include <ranges>

namespace aniparse {

static void apply_config_to(ClientRequest& any_request, const ParserConfig& config) {
	std::visit([&](auto& request) {
		// Request headers take priority over config: merge_missing keeps existing names.
		request.headers.merge_missing(config.headers);

		for (const auto& [key, value] : config.url_params) {
			request.url_params.add(key, value);
		}
	}, any_request);

	for (auto& modifier : config.modifiers) {
		modifier(any_request);
	}
}

RequestorContext::RequestorContext(std::shared_ptr<const ServiceState> services,
                                   std::shared_ptr<ParserConfig> config)
    : services_(std::move(services))
    , config_(config ? std::move(config) : std::make_shared<ParserConfig>()) {
	if (!services_ || !services_->client) {
		throw std::invalid_argument("Client has to be valid");
	}
	// resources() hands out a reference, so a cache must always exist. Fill one in
	// if the caller left it null, copying the state only in that (rare) case.
	if (!services_->resources) {
		auto filled = std::make_shared<ServiceState>(*services_);
		filled->resources = std::make_shared<ResourceCache>();
		services_ = std::move(filled);
	}
	// Provision a jar only if the config does not already carry one. Overwriting
	// would drop a session established earlier (login, or a restored jar) and
	// would make new_with_logger/new_with_config silently wipe cookies.
	if (!config_->cookie_jar) {
		config_->cookie_jar = services_->client->make_cookie_jar();
	}

	// Snapshot mirror and canonical-origin overrides once, so every source URL
	// this context resolves reads the same set even if a catalog apply swaps the
	// holder mid-operation.
	if (services_->mirrors) {
		mirror_snapshot_ = services_->mirrors->get();
	}

	// Post-construction invariant every other method relies on: a valid service
	// bundle with a client and a resource cache, and a valid config. The cookie
	// jar is best-effort (a client's make_cookie_jar may legitimately return null),
	// so it is not part of the invariant.
	assert(services_ && services_->client && services_->resources);
	assert(config_);
}

RequestorContext::RequestorContext(std::shared_ptr<ClientContext> client,
                                   std::shared_ptr<LoggerContext> logger,
                                   std::shared_ptr<ParserConfig> config)
    : RequestorContext(std::make_shared<ServiceState>(ServiceState{
                           .client = std::move(client),
                           .logger = std::move(logger),
                       }),
                       std::move(config)) {}

NetworkRequestTask<ResponseData> RequestorContext::request(GetRequest request) {
	assert(config_ && services_ && services_->client);
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<GetRequest>(any_request));

	return services_->client->do_request(ConfiguredGetRequest{
	    .request = std::get<GetRequest>(std::move(any_request)),
	    .cookies = config_->cookie_jar});
}

NetworkRequestTask<ResponseData> RequestorContext::request(PostRequest request) {
	assert(config_ && services_ && services_->client);
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<PostRequest>(any_request));

	return services_->client->do_request(ConfiguredPostRequest{
	    .request = std::get<PostRequest>(std::move(any_request)),
	    .cookies = config_->cookie_jar});
}

NetworkRequestTask<ResponseData> RequestorContext::request(PostMultipartRequest request) {
	assert(config_ && services_ && services_->client);
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<PostMultipartRequest>(any_request));

	return services_->client->do_request(ConfiguredPostMultipartRequest{
	    .request = std::get<PostMultipartRequest>(std::move(any_request)),
	    .cookies = config_->cookie_jar});
}

namespace {

bool is_success_status(long status) {
	return status >= 200 && status < 300;
}

// Map a non-2xx status onto a coarse RequestErrorCode. The retry policy (429/5xx)
// lives below the helpers in the client, so by the time a helper sees these the
// policy is exhausted — RateLimited/ServerError mean "already retried".
RequestErrorCode status_to_error_code(long status) {
	switch (status) {
	case 401:
	case 403:
		return RequestErrorCode::InvalidCredentials;
	case 404:
		return RequestErrorCode::NotFound;
	case 429:
		return RequestErrorCode::RateLimited;
	default:
		return status >= 500 ? RequestErrorCode::ServerError : RequestErrorCode::Unknown;
	}
}

// Perform the request and require a 2xx: forwards a transport error from request()
// unchanged, maps a non-2xx status via status_to_error_code, and on success yields
// the raw 2xx response for the caller to parse. Templated on the request type so
// the GET and POST helpers share it (the matching request() overload is picked).
template <typename Request>
NetworkRequestTask<ResponseData> fetch_ok(RequestorContext& context, Request request) {
	auto fetched = co_await context.request(std::move(request));
	if (!fetched) {
		co_return unexpected(std::move(fetched.error()));
	}
	ResponseData& response = *fetched;

	if (!is_success_status(response.status_code)) {
		co_return make_response_error(status_to_error_code(response.status_code),
		                              "HTTP " + std::to_string(response.status_code),
		                              response.status_code, std::move(response.body));
	}
	co_return std::move(response);
}

template <typename Request>
NetworkRequestTask<html::HTMLDocument> request_html_impl(RequestorContext& context, Request request) {
	auto fetched = co_await fetch_ok(context, std::move(request));
	if (!fetched) {
		co_return unexpected(std::move(fetched.error()));
	}
	ResponseData& response = *fetched;

	html::HTMLParser parser;
	auto document = parser.try_parse(response.body);
	if (!document) {
		co_return make_response_error(RequestErrorCode::UnexpectedResponse,
		                              std::string("HTML parse failed: ") + document.error().what(),
		                              response.status_code, std::move(response.body));
	}
	co_return std::move(*document);
}

template <typename Request>
NetworkRequestTask<boost::json::value> request_json_impl(RequestorContext& context, Request request) {
	auto fetched = co_await fetch_ok(context, std::move(request));
	if (!fetched) {
		co_return unexpected(std::move(fetched.error()));
	}
	ResponseData& response = *fetched;

	boost::system::error_code ec;
	boost::json::value value = boost::json::parse(response.body, ec);
	if (ec) {
		co_return make_response_error(RequestErrorCode::UnexpectedResponse,
		                              "JSON parse failed: " + ec.message(),
		                              response.status_code, std::move(response.body));
	}
	co_return std::move(value);
}

} // namespace

NetworkRequestTask<html::HTMLDocument> RequestorContext::request_html(GetRequest request) {
	return request_html_impl(*this, std::move(request));
}

NetworkRequestTask<html::HTMLDocument> RequestorContext::request_html(PostRequest request) {
	return request_html_impl(*this, std::move(request));
}

NetworkRequestTask<boost::json::value> RequestorContext::request_json(GetRequest request) {
	return request_json_impl(*this, std::move(request));
}

NetworkRequestTask<boost::json::value> RequestorContext::request_json(PostRequest request) {
	return request_json_impl(*this, std::move(request));
}

std::shared_ptr<const ParserConfig> RequestorContext::config() const {
	assert(config_);
	return config_;
}

ResourceCache& RequestorContext::resources() const {
	assert(services_ && services_->resources);
	return *services_->resources;
}

std::shared_ptr<const html::SelectorSource> RequestorContext::selector_source() const {
	assert(services_);
	if (services_->selectors) {
		if (auto source = services_->selectors->get()) {
			return source;
		}
	}
	// No holder (the default, until the volatile index populates one): a shared
	// empty source, so every selector falls back to its built-in literal.
	static const std::shared_ptr<const html::SelectorSource> empty =
	    std::make_shared<const html::SelectorSource>();
	return empty;
}

std::shared_ptr<const RegexSource> RequestorContext::pattern_source() const {
	assert(services_);
	if (services_->patterns) {
		if (auto source = services_->patterns->get()) {
			return source;
		}
	}
	// No holder: a shared empty source, so every pattern falls back to its
	// built-in literal.
	static const std::shared_ptr<const RegexSource> empty =
	    std::make_shared<const RegexSource>();
	return empty;
}

size_t RequestorContext::alt_link() const {
	assert(config_);
	return config_->alt_link;
}

void RequestorContext::set_alt_link(size_t alt_link) {
	assert(config_);
	config_->alt_link = alt_link;
}

RequestorContext RequestorContext::new_with_logger(std::shared_ptr<LoggerContext> logger) const {
	assert(services_);
	// A different logger is a different service bundle: copy the state and swap it.
	auto services = std::make_shared<ServiceState>(*services_);
	services->logger = std::move(logger);
	return RequestorContext(std::move(services), config_);
}

RequestorContext RequestorContext::new_with_config(std::shared_ptr<ParserConfig> config) const {
	assert(services_);
	// Only the config changes; the services are shared as-is (no allocation).
	return RequestorContext(services_, std::move(config));
}

}; // namespace aniparse
