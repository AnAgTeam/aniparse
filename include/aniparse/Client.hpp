#pragma once
#include "aniparse/Requests.hpp"
#include "aniparse/ClientContext.hpp"

#include <asyncnet/AsyncSession.hpp>

namespace aniparse {

	/**
	 * @brief Class for performing asyncronous HTTP requests
	 * (C++20 coroutine based)
	 * Can be used as client for parsers. @see RequestorContext
	 */
	class AsyncClient : public ClientContext {
	public:
		/**
		 * @bief Initialize client
		 */
		AsyncClient();

		/**
		 * @bief Perform HTTP GET request
		 * @param request HTTP request
		 * @return Task with response
		 */
		asyncnet::NetworkTask<asyncnet::Response> do_request(GetRequest request);

		/**
		 * @bief Perform HTTP POST request
		 * @param request HTTP request
		 * @return Task with response
		 */
		asyncnet::NetworkTask<asyncnet::Response> do_request(PostRequest request);

		/**
		 * @bief Perform HTTP POST multipart/form-data request
		 * @param request HTTP request
		 * @return Task with response
		 */
		asyncnet::NetworkTask<asyncnet::Response> do_request(PostMultipartRequest request);

		/**
		 * @bief Perform custom HTTP request
		 * @param request HTTP request
		 * @return Task with response
		 */
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
		
		void set_user_agent(std::string_view user_agent);

	private:

		std::list<std::string> get_default_headers();

		asyncnet::AsyncSession session_;
	};
}