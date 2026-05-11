/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Requests.hpp"
#include "aniparse/utility/Format.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <asyncnet/Response.hpp>

namespace aniparse {

	using ClientConfigFlags = FlagsBitfield<64, struct ClientConfigFlagsTag>;

	namespace client_flags {
		inline constexpr auto follow_redirects = ClientConfigFlags::make_bit(0);
		inline constexpr auto verify_ssl       = ClientConfigFlags::make_bit(1);
	}

	struct ClientProxy {
		std::string user, passwd;
		std::string host;
		uint16_t port = 0;
	};

	struct ClientConfig {
		std::map<std::string, std::string> headers;
		std::map<std::string, std::string> url_params;
		ClientConfigFlags flags;
		std::optional<std::chrono::milliseconds> timeout;
		std::optional<ClientProxy> proxy;
		std::optional<uint32_t> max_retries;

		std::vector<std::function<void(aniparse::ClientRequest&)>> modifiers;
	};

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

	enum class LogLevel {
		Debug,
		Info,
		Warning,
		Error,
		Fatal
	};

	/**
	 * @brief Interface for logging messages
	 */
	struct LoggerContext {
		virtual ~LoggerContext() = default;

		/**
		 * @brief Output text with prefix and message
		 * @param message_type Prefix to message. ("INFO", "ERROR", etc.)
		 * @param message Message text to output
		 */
		virtual void log(LogLevel message_type,
			std::string_view message,
			const std::source_location loc = std::source_location::current()) = 0;

		/**
		 * @brief Output INFO text
		 * @param message Message text to output
		 */
		void log_info(std::string_view message,
			const std::source_location loc = std::source_location::current());

		/**
		 * @brief Output DEBUG text
		 * @param message Message text to output
		 */
		void log_debug(std::string_view message,
			const std::source_location loc = std::source_location::current());

		/**
		 * @brief Output ERROR text
		 * @param message Message text to output
		 */
		void log_error(std::string_view message,
			const std::source_location loc = std::source_location::current());
	};

	/**
	 * @brief General context for parsers with client, its config and logger
	 */
	class RequestorContext {
	public:
		//constexpr RequestorContext() noexcept = default;

		/**
		 * @brief Construct new context for parsers
		 * @param client Client for HTTP requests. Should be valid
		 * @param logger Optional message logger
		 * @param config Optional config for HTTP client
		 * @note Even if passed config is nullptr, config() returns always valid config.
		 * @throws std::invalid_argument if client is nullptr
		 */
		RequestorContext(std::shared_ptr<ClientContext> client,
			std::shared_ptr<LoggerContext> logger,
			std::shared_ptr<ClientConfig> config);

		/**
		 * @bief Perform HTTP GET request with respect to client config
		 * @param request HTTP request
		 * @return Task with response
		 */
		asyncnet::NetworkTask<asyncnet::Response> request(GetRequest request);

		/**
		 * @bief Perform HTTP POST request with respect to client config
		 * @param request HTTP request
		 * @return Task with response
		 */
		asyncnet::NetworkTask<asyncnet::Response> request(PostRequest request);

		/**
		 * @bief Perform HTTP POST multipart/form-data request with respect to client config
		 * @param request HTTP request
		 * @return Task with response
		 */
		asyncnet::NetworkTask<asyncnet::Response> request(PostMultipartRequest request);

		/**
		 * @todo
		 */
		asyncnet::NetworkTask<asyncnet::Response> request(std::shared_ptr<PolymorphicRequest> request);

		/**
		 * @brief Output INFO to logger
		 * @tparam Args Format arguments types
		 * @param fmt Format string
		 * @param args Format arguments
		 */
		template<typename ... Args>
		void info(SourcedFormatString<Args ...> sourced_fmt, Args&& ... args) {
			if (logger_) {
				logger_->log(LogLevel::Info,
					format(sourced_fmt.fmt, std::forward<Args>(args) ...),
					sourced_fmt.loc);
			}
		}

		/**
		 * @brief Output DEBUG to logger
		 * @tparam Args Format arguments types
		 * @param fmt Format string
		 * @param args Format arguments
		 */
		template<typename ... Args>
		void debug(SourcedFormatString<Args ...> sourced_fmt, Args&& ... args) {
			if (logger_) {
				logger_->log(LogLevel::Debug,
					format(sourced_fmt.fmt, std::forward<Args>(args) ...),
					sourced_fmt.loc);
			}
		}

		/**
		 * @brief Output WARNING to logger
		 * @tparam Args Format arguments types
		 * @param fmt Format string
		 * @param args Format arguments
		 */
		template<typename ... Args>
		void warning(SourcedFormatString<Args ...> sourced_fmt, Args&& ... args) {
			if (logger_) {
				logger_->log(LogLevel::Warning,
					format(sourced_fmt.fmt, std::forward<Args>(args) ...),
					sourced_fmt.loc);
			}
		}

		/**
		 * @brief Output ERROR to logger
		 * @tparam Args Format arguments types
		 * @param fmt Format string
		 * @param args Format arguments
		 */
		template<typename ... Args>
		void error(SourcedFormatString<Args ...> sourced_fmt, Args&& ... args) {
			if (logger_) {
				logger_->log(LogLevel::Error,
					format(sourced_fmt.fmt, std::forward<Args>(args) ...),
					sourced_fmt.loc);
			}
		}

		/**
		 * @brief Output FATAL to logger
		 * @tparam Args Format arguments types
		 * @param fmt Format string
		 * @param args Format arguments
		 */
		template<typename ... Args>
		void fatal(SourcedFormatString<Args ...> sourced_fmt, Args&& ... args) {
			if (logger_) {
				logger_->log(LogLevel::Fatal,
					format(sourced_fmt.fmt, std::forward<Args>(args) ...),
					sourced_fmt.loc);
			}
		}

		/**
		 * @brief Returns HTTP client configuration. Always valie
		 * @return HTTP client configuration
		 */
		std::shared_ptr<const ClientConfig> config() const;

		size_t alt_link() const;
		void set_alt_link(size_t alt_link);

		//std::shared_ptr<LoggerContext> logger() const;
		//void set_logger(std::shared_ptr<LoggerContext> logger);


		/**
		 * @brief Make new context for parsers with new logger
		 * @return New context with inherited properties and new logger
		 */
		RequestorContext new_with_logger(std::shared_ptr<LoggerContext> logger) const;

		/**
		 * @brief Make new context for parsers with new client
		 * @return New context with inherited properties and new client
		 */
		RequestorContext new_with_client(std::shared_ptr<ClientContext> client) const;

		/**
		 * @brief Make new context for parsers with new HTTP client config
		 * @return New context with inherited properties and new HTTP client config
		 */
		RequestorContext new_with_config(std::shared_ptr<ClientConfig> config) const;

	private:
		std::shared_ptr<ClientContext> client_;
		std::shared_ptr<LoggerContext> logger_;
		std::shared_ptr<ClientConfig> config_;
		size_t alt_link_ = 0;
	};
}