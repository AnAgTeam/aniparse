/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
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

		void set_timeout(const std::optional<std::chrono::system_clock::duration>& timeout);
		
		void set_user_agent(std::string_view user_agent);

	private:

		std::list<std::string> get_default_headers();

		asyncnet::AsyncSession session_;
	};
}