/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/runtime/RequestContext.hpp"
#include "aniparse/cache/ResourceCache.hpp"
#include "aniparse/html/HTMLParser.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <cassert>
#include <algorithm>
#include <ranges>

namespace aniparse {

static void apply_config_to(HttpRequest& any_request, const ParserConfig& config,
                            const RequestDefaults& defaults, const RequestContext& context) {
	std::visit([&](auto& request) {
		// Request headers take priority over config: merge_missing keeps existing names.
		request.headers.merge_missing(config.headers);
		request.headers.merge_missing(defaults.headers);

		for (const auto& [key, value] : config.url_params) {
			request.url_params.add(key, value);
		}
	}, any_request);

	for (auto& modifier : config.modifiers) {
		modifier(any_request, context);
	}
}

std::shared_ptr<const OverridesState> RequestContext::capture_overrides(const RuntimeServices& services) {
    if (services.overrides) {
        if (auto current = services.overrides->load()) return current;
    }
    static const auto empty = std::make_shared<const OverridesState>();
    return empty;
}

RequestContext::RequestContext(std::shared_ptr<const RuntimeServices> services,
                               std::shared_ptr<const detail::SessionState> state,
                               std::shared_ptr<const OverridesState> overrides,
                               std::shared_ptr<const RequestDefaults> defaults)
    : services_(std::move(services)), state_(std::move(state)),
      overrides_(std::move(overrides)), defaults_(std::move(defaults)) {
    resolve_urls();
}

Mirrors RequestContext::mirrors() const {
    const auto& binding = *state_->binding;
    const auto* urls = overrides_->mirrors->list_for(binding.parser_id, binding.profile_id);
    return Mirrors{urls, binding.mirror_views};
}

void RequestContext::resolve_urls() {
    base_url_ = state_->settings.base_url ? std::string_view(*state_->settings.base_url) : mirrors().base_url(0);
    const auto& binding = *state_->binding;
    const auto* frontend = overrides_->mirrors->canonical_base_for(binding.parser_id, binding.profile_id);
    canonical_base_url_ = state_->settings.canonical_base_url ? std::string_view(*state_->settings.canonical_base_url) :
        (frontend ? std::string_view(*frontend) : std::string_view(binding.canonical_base_url));
}

RequestContext RequestContext::make_extractor(std::shared_ptr<const RuntimeServices> services,
                                               std::string extractor_id) {
    ParserConfig config;
    if (!services || !services->client) throw std::invalid_argument("Client has to be valid");
    if (!services->resources) {
        auto filled = std::make_shared<RuntimeServices>(*services);
        filled->resources = std::make_shared<ResourceCache>();
        services = std::move(filled);
    }
    if (!config.cookie_jar) config.cookie_jar = services->client->make_cookie_jar();
    auto binding = std::make_shared<detail::ProfileBinding>();
    binding->parser_id = std::move(extractor_id);
    auto state = std::make_shared<const detail::SessionState>(detail::SessionState{
        std::move(binding), std::make_shared<const ParserConfig>(std::move(config)), {}});
    auto overrides = capture_overrides(*services);
    auto defaults = services->client->request_defaults_snapshot();
    return RequestContext(std::move(services), std::move(state), std::move(overrides), std::move(defaults));
}

RequestTask<HttpResponse> RequestContext::request(GetRequest request) {
	assert(state_ && services_ && services_->client);
	HttpRequest any_request = std::move(request);
	apply_config_to(any_request, *state_->config, *defaults_, *this);
	assert(std::holds_alternative<GetRequest>(any_request));

	return services_->client->do_request(ConfiguredGetRequest{
	    .request = std::get<GetRequest>(std::move(any_request)),
	    .cookies = state_->config->cookie_jar});
}

RequestTask<HttpResponse> RequestContext::request(PostRequest request) {
	assert(state_ && services_ && services_->client);
	HttpRequest any_request = std::move(request);
	apply_config_to(any_request, *state_->config, *defaults_, *this);
	assert(std::holds_alternative<PostRequest>(any_request));

	return services_->client->do_request(ConfiguredPostRequest{
	    .request = std::get<PostRequest>(std::move(any_request)),
	    .cookies = state_->config->cookie_jar});
}

RequestTask<HttpResponse> RequestContext::request(PostMultipartRequest request) {
	assert(state_ && services_ && services_->client);
	HttpRequest any_request = std::move(request);
	apply_config_to(any_request, *state_->config, *defaults_, *this);
	assert(std::holds_alternative<PostMultipartRequest>(any_request));

	return services_->client->do_request(ConfiguredPostMultipartRequest{
	    .request = std::get<PostMultipartRequest>(std::move(any_request)),
	    .cookies = state_->config->cookie_jar});
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
RequestTask<HttpResponse> fetch_ok(RequestContext& context, Request request) {
	auto fetched = co_await context.request(std::move(request));
	if (!fetched) {
		co_return unexpected(std::move(fetched.error()));
	}
	HttpResponse& response = *fetched;

	if (!is_success_status(response.status_code)) {
		co_return make_request_error(status_to_error_code(response.status_code),
		                              "HTTP " + std::to_string(response.status_code),
		                              response.status_code, std::move(response.body));
	}
	co_return std::move(response);
}

template <typename Request>
RequestTask<html::HTMLDocument> request_html_impl(RequestContext& context, Request request) {
	auto fetched = co_await fetch_ok(context, std::move(request));
	if (!fetched) {
		co_return unexpected(std::move(fetched.error()));
	}
	HttpResponse& response = *fetched;

	html::HTMLParser parser;
	auto document = parser.try_parse(response.body);
	if (!document) {
		co_return make_request_error(RequestErrorCode::UnexpectedResponse,
		                              std::string("HTML parse failed: ") + document.error().what(),
		                              response.status_code, std::move(response.body));
	}
	co_return std::move(*document);
}

template <typename Request>
RequestTask<boost::json::value> request_json_impl(RequestContext& context, Request request) {
	auto fetched = co_await fetch_ok(context, std::move(request));
	if (!fetched) {
		co_return unexpected(std::move(fetched.error()));
	}
	HttpResponse& response = *fetched;

	boost::system::error_code ec;
	boost::json::value value = boost::json::parse(response.body, ec);
	if (ec) {
		co_return make_request_error(RequestErrorCode::UnexpectedResponse,
		                              "JSON parse failed: " + ec.message(),
		                              response.status_code, std::move(response.body));
	}
	co_return std::move(value);
}

} // namespace

RequestTask<html::HTMLDocument> RequestContext::request_html(GetRequest request) {
	return request_html_impl(*this, std::move(request));
}

RequestTask<html::HTMLDocument> RequestContext::request_html(PostRequest request) {
	return request_html_impl(*this, std::move(request));
}

RequestTask<boost::json::value> RequestContext::request_json(GetRequest request) {
	return request_json_impl(*this, std::move(request));
}

RequestTask<boost::json::value> RequestContext::request_json(PostRequest request) {
	return request_json_impl(*this, std::move(request));
}

std::shared_ptr<const ParserConfig> RequestContext::config() const {
	assert(state_);
	return state_->config;
}

ResourceCache& RequestContext::resources() const {
	assert(services_ && services_->resources);
	return *services_->resources;
}

} // namespace aniparse
