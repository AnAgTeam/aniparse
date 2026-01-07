#include "aniparse/Client.hpp"


using asyncnet::NetworkTask;
using asyncnet::Response;

namespace aniparse {
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

	NetworkTask<Response> AsyncClient::do_request(PostMultipartRequest request) {
		throw std::logic_error("Not implemented");
	}

	NetworkTask<Response> AsyncClient::do_request(std::shared_ptr<PolymorphicRequest> request) {
		throw std::logic_error("Not implemented");
	}

}