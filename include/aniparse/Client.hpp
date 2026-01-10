#pragma once
#include "aniparse/Requests.hpp"
#include "aniparse/ClientContext.hpp"

#include <asyncnet/AsyncSession.hpp>

namespace aniparse {

	class AsyncClient : ClientContext {
	public:
		AsyncClient() = default;

		asyncnet::NetworkTask<asyncnet::Response> do_request(GetRequest request);
		asyncnet::NetworkTask<asyncnet::Response> do_request(PostRequest request);
		asyncnet::NetworkTask<asyncnet::Response> do_request(PostMultipartRequest request);
		asyncnet::NetworkTask<asyncnet::Response> do_request(std::shared_ptr<PolymorphicRequest> request);

		template<std::same_as<ClientRequest> T>
		asyncnet::NetworkTask<asyncnet::Response> do_request(T client_request) {
			return std::visit([this](auto& request) {
				return do_request(std::move(request));
			}, client_request);
		}

		template<typename T>
		asyncnet::NetworkTask<T> do_request(ClientParsedRequest<T> request) {
			asyncnet::Response resp = co_await do_request(std::move(request.request));
			co_return request.parse(std::move(resp));
		}

		template<typename T>
		asyncnet::NetworkTask<T> do_request(OptionalRequest<T> request) {
			if (std::holds_alternative<T>(request)) {
				co_return std::get<T>(std::move(request));
			}
			co_return co_await do_request(std::get<ClientParsedRequest<T>>(std::move(request)));
		}

		void set_timeout(const std::optional<std::chrono::system_clock::duration>& timeout);

	private:
		asyncnet::AsyncSession session_;
	};
}