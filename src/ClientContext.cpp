/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Format.hpp"
#include <ranges>

template <class... Ts>
struct Overloaded : Ts... {
	using Ts::operator()...;
};

namespace aniparse {

static void apply_config_to(ClientRequest& any_request, const ParserConfig& config) {
	std::visit(Overloaded{
	               [](std::shared_ptr<PolymorphicRequest>& request) {},
	               [&](auto& request) {
		               // Request headers take priority over config: insert() keeps existing keys.
		               request.headers.insert(config.headers.begin(), config.headers.end());

		               for (auto& param : config.url_params) {
			               request.url_params += param;
		               }
	               }},
	           any_request);

	for (auto& modifier : config.modifiers) {
		modifier(any_request);
	}
}

RequestorContext::RequestorContext(std::shared_ptr<ClientContext> client,
                                   std::shared_ptr<LoggerContext> logger,
                                   std::shared_ptr<ParserConfig> config)
    : client_(std::move(client))
    , logger_(std::move(logger))
    , config_(config ? std::move(config) : std::make_shared<ParserConfig>()) {
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

asyncnet::NetworkTask<asyncnet::Response> RequestorContext::request(GetRequest request) {
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<GetRequest>(any_request));

	return client_->do_request(ConfiguredGetRequest{
	    .request = std::get<GetRequest>(std::move(any_request)),
	    .cookies = config_->cookie_jar});
}

asyncnet::NetworkTask<asyncnet::Response> RequestorContext::request(PostRequest request) {
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<PostRequest>(any_request));

	return client_->do_request(ConfiguredPostRequest{
	    .request = std::get<PostRequest>(std::move(any_request)),
	    .cookies = config_->cookie_jar});
}

asyncnet::NetworkTask<asyncnet::Response> RequestorContext::request(PostMultipartRequest request) {
	ClientRequest any_request = std::move(request);
	apply_config_to(any_request, *config_);
	assert(std::holds_alternative<PostMultipartRequest>(any_request));

	return client_->do_request(ConfiguredPostMultipartRequest{
	    .request = std::get<PostMultipartRequest>(std::move(any_request)),
	    .cookies = config_->cookie_jar});
}

asyncnet::NetworkTask<asyncnet::Response> RequestorContext::request(std::shared_ptr<PolymorphicRequest> request) {
	throw std::runtime_error("Unsupported");
}

std::shared_ptr<const ParserConfig> RequestorContext::config() const {
	return config_;
}

size_t RequestorContext::alt_link() const {
	return config_->alt_link;
}

void RequestorContext::set_alt_link(size_t alt_link) {
	config_->alt_link = alt_link;
}

RequestorContext RequestorContext::new_with_logger(std::shared_ptr<LoggerContext> logger) const {
	return RequestorContext(client_, logger, config_);
}

//RequestorContext RequestorContext::new_with_client(std::shared_ptr<ClientContext> client) const {
//    return RequestorContext(client, logger_, config_);
//}

RequestorContext RequestorContext::new_with_config(std::shared_ptr<ParserConfig> config) const {
	return RequestorContext(client_, logger_, config);
}

}; // namespace aniparse
