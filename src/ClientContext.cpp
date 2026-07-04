/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/ClientContext.hpp"
#include "aniparse/ResourceCache.hpp"
#include "aniparse/html/HTMLParser.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <ranges>

namespace aniparse {

static void apply_config_to(ClientRequest& any_request, const ParserConfig& config) {
	std::visit([&](auto& request) {
		// Request headers take priority over config: insert() keeps existing keys.
		request.headers.insert(config.headers.begin(), config.headers.end());

		for (const auto& [key, value] : config.url_params) {
			request.url_params.add(key, value);
		}
	}, any_request);

	for (auto& modifier : config.modifiers) {
		modifier(any_request);
	}
}

RequestorContext::RequestorContext(std::shared_ptr<ClientContext> client,
                                   std::shared_ptr<LoggerContext> logger,
                                   std::shared_ptr<ParserConfig> config,
                                   std::shared_ptr<ResourceCache> resources)
    : client_(std::move(client))
    , logger_(std::move(logger))
    , config_(config ? std::move(config) : std::make_shared<ParserConfig>())
    , resources_(resources ? std::move(resources) : std::make_shared<ResourceCache>()) {
	if (!client_) {
		throw std::invalid_argument("Client has to be valid");
	}
	// Provision a jar only if the config does not already carry one. Overwriting
	// would drop a session established earlier (login, or a restored jar) and
	// would make new_with_logger/new_with_config silently wipe cookies.
	if (!config_->cookie_jar) {
		config_->cookie_jar = client_->make_cookie_jar();
	}
}

NetworkRequestTask<ResponseData> RequestorContext::request(GetRequest request) {
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<GetRequest>(any_request));

	return client_->do_request(ConfiguredGetRequest{
	    .request = std::get<GetRequest>(std::move(any_request)),
	    .cookies = config_->cookie_jar});
}

NetworkRequestTask<ResponseData> RequestorContext::request(PostRequest request) {
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<PostRequest>(any_request));

	return client_->do_request(ConfiguredPostRequest{
	    .request = std::get<PostRequest>(std::move(any_request)),
	    .cookies = config_->cookie_jar});
}

NetworkRequestTask<ResponseData> RequestorContext::request(PostMultipartRequest request) {
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<PostMultipartRequest>(any_request));

	return client_->do_request(ConfiguredPostMultipartRequest{
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
	return config_;
}

ResourceCache& RequestorContext::resources() const {
	return *resources_;
}

size_t RequestorContext::alt_link() const {
	return config_->alt_link;
}

void RequestorContext::set_alt_link(size_t alt_link) {
	config_->alt_link = alt_link;
}

RequestorContext RequestorContext::new_with_logger(std::shared_ptr<LoggerContext> logger) const {
	return RequestorContext(client_, logger, config_, resources_);
}

//RequestorContext RequestorContext::new_with_client(std::shared_ptr<ClientContext> client) const {
//    return RequestorContext(client, logger_, config_);
//}

RequestorContext RequestorContext::new_with_config(std::shared_ptr<ParserConfig> config) const {
	return RequestorContext(client_, logger_, config, resources_);
}

}; // namespace aniparse
