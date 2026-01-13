#pragma once
#include "aniparse/Requests.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <asyncnet/Response.hpp>

namespace aniparse {
	struct ClientContext {
		virtual ~ClientContext() = default;

		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(GetRequest request) = 0;
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(PostRequest request) = 0;
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(PostMultipartRequest request) = 0;
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(std::shared_ptr<PolymorphicRequest> request) = 0;
	};
}