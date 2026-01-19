/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#pragma once
#include "aniparse/Requests.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <asyncnet/Response.hpp>

namespace aniparse {

	/**
	 * @brief Interface for performing HTTP requests
	 */
	struct ClientContext {
		virtual ~ClientContext() = default;

		/**
		 * @bief Perform HTTP GET request
		 * @param request HTTP request
		 * @return Task with response
		 */
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(GetRequest request) = 0;

		/**
		 * @bief Perform HTTP POST request
		 * @param request HTTP request
		 * @return Task with response
		 */
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(PostRequest request) = 0;
		
		/**
		 * @bief Perform HTTP POST multipart/form-data request
		 * @param request HTTP request
		 * @return Task with response
		 */
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(PostMultipartRequest request) = 0;
		
		/**
		 * @bief Perform custom HTTP request
		 * @param request HTTP request
		 * @return Task with response
		 */
		virtual asyncnet::NetworkTask<asyncnet::Response> do_request(std::shared_ptr<PolymorphicRequest> request) = 0;
	};

	/**
	 * @brief Interface for logging messages
	 */
	struct LoggerContext {
		virtual ~LoggerContext() = default;

		/**
		 * @bief Output text with prefix and message
		 * @param message_type Prefix to message. ("INFO", "ERROR", etc.)
		 * @param message Message text to output
		 */
		virtual void log(std::string_view message_type, std::string_view message) = 0;

		/**
		 * @bief Output INFO text
		 * @param message Message text to output
		 */
		void log_info(std::string_view message);

		/**
		 * @bief Output DEBUG text
		 * @param message Message text to output
		 */
		void log_debug(std::string_view message);

		/**
		 * @bief Output ERROR text
		 * @param message Message text to output
		 */
		void log_error(std::string_view message);
	};

	struct RequestorContext {
		std::shared_ptr<ClientContext> client;
		std::shared_ptr<LoggerContext> logger;
		size_t alt_link = 0;
	};
}