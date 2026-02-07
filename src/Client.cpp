/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/Client.hpp"


using asyncnet::NetworkTask;
using asyncnet::Response;

namespace aniparse {
	AsyncClient::AsyncClient() {
		session_.set_default_headers(get_default_headers());
	}

	NetworkTask<Response> AsyncClient::do_request(GetRequest request) {
		auto get_request = session_.make_request<asyncnet::GetRequest>(std::move(request.url));
		get_request.add_headers(std::move(request.headers));
		get_request.set_url_parameters(std::move(request.url_params));
		co_return co_await session_.perform_request(get_request);
	}

	NetworkTask<Response> AsyncClient::do_request(PostRequest request) {
		auto post_request = session_.make_request<asyncnet::PostRequest>(std::move(request.url), std::move(request.body));
		post_request.add_headers(std::move(request.headers));
		post_request.set_url_parameters(std::move(request.url_params));
		co_return co_await session_.perform_request(post_request);
	}

	NetworkTask<Response> AsyncClient::do_request(PostMultipartRequest) {
		throw std::logic_error("Not implemented");
	}

	NetworkTask<Response> AsyncClient::do_request(std::shared_ptr<PolymorphicRequest>) {
		throw std::logic_error("Not implemented");
	}

	void AsyncClient::set_user_agent(std::string_view user_agent) {
		session_.set_default_headers({
			"User-Agent: " + std::string(user_agent)
		});
	}

	std::list<std::string> AsyncClient::get_default_headers() {
		return {
			"User-Agent: Libaniparse/0.1"
		};
	}

}