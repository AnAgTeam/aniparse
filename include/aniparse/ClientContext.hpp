#pragma once
#include "aniparse/Requests.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <asyncnet/Response.hpp>

namespace aniparse {
	struct ClientContext {
		virtual ~ClientContext() = default;

		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(GetRequest request);
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(PostRequest request);
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(PostMultipartRequest request);
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(std::shared_ptr<PolymorphicRequest> request);
	};
}