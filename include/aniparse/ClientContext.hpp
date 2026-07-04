/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Request.hpp"
#include "aniparse/types/Response.hpp"
#include "aniparse/CookieJar.hpp"
#include "aniparse/utility/Format.hpp"

#include <asyncnet/CancellingTask.hpp>

namespace aniparse {

class ResourceCache;

using ClientConfigFlags = FlagsBitfield<64, struct ClientConfigFlagsTag>;
using ParserConfigFlags = FlagsBitfield<64, struct ParserConfigFlagsTag>;

namespace client_config_flags {
inline constexpr auto verify_ssl = ClientConfigFlags::make_bit(0);
} // namespace client_config_flags

namespace parser_config_flags {
inline constexpr auto follow_redirects = ParserConfigFlags::make_bit(0);
} // namespace parser_config_flags

struct ClientProxy {
	std::string user, passwd;
	std::string host;
	uint16_t port = 0;
};

/**
 * @brief Parser configuration for all its requests
 * @see RequestorContext
 */
struct ParserConfig {
	Headers headers;
	std::map<std::string, std::string> url_params;
	std::shared_ptr<CookieJar> cookie_jar;
	ParserConfigFlags flags;

	/// Selected alternative link (mirror) index. @see AltLink, RequestorContext::alt_link
	size_t alt_link = 0;

	std::vector<std::function<void(aniparse::ClientRequest&)>> modifiers;
};

struct ClientConfig {
	std::optional<ClientProxy> proxy;
	std::optional<uint32_t> max_retries;
	std::optional<std::chrono::milliseconds> timeout;
	ClientConfigFlags flags;
};

/**
 * @brief Interface for performing HTTP requests
 */
struct ClientContext {
	virtual ~ClientContext() = default;

	/**
	 * @brief Perform HTTP GET request
	 * @param request HTTP request
	 * @return Task with response
	 */
	virtual asyncnet::NetworkTask<ResponseData> do_request(ConfiguredGetRequest request) = 0;

	/**
	 * @brief Perform HTTP POST request
	 * @param request HTTP request
	 * @return Task with response
	 */
	virtual asyncnet::NetworkTask<ResponseData> do_request(ConfiguredPostRequest request) = 0;

	/**
	 * @brief Perform HTTP POST multipart/form-data request
	 * @param request HTTP request
	 * @return Task with response
	 */
	virtual asyncnet::NetworkTask<ResponseData> do_request(ConfiguredPostMultipartRequest request) = 0;

	/**
	 * @todo docs
	 */
	virtual void set_config(ClientConfig config) = 0;

	virtual std::shared_ptr<CookieJar> make_cookie_jar() = 0;
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
};

/**
 * @brief General context for parsers with client, its config and logger
 * @see ParserConfig
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
	                 std::shared_ptr<ParserConfig> config,
	                 std::shared_ptr<ResourceCache> resources = nullptr);

	/**
	 * @brief Perform HTTP GET request with respect to client config
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<ResponseData> request(GetRequest request);

	/**
	 * @brief Perform HTTP POST request with respect to client config
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<ResponseData> request(PostRequest request);

	/**
	 * @brief Perform HTTP POST multipart/form-data request with respect to client config
	 * @param request HTTP request
	 * @return Task with response
	 */
	asyncnet::NetworkTask<ResponseData> request(PostMultipartRequest request);

	/**
	 * @todo
	 */
	asyncnet::NetworkTask<ResponseData> request(std::shared_ptr<PolymorphicRequest> request);

	/**
	 * @brief Output INFO to logger
	 * @tparam Args Format arguments types
	 * @param fmt Format string
	 * @param args Format arguments
	 */
	template <typename... Args>
	void info(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (logger_) {
			logger_->log(LogLevel::Info,
			             format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Output DEBUG to logger
	 * @tparam Args Format arguments types
	 * @param fmt Format string
	 * @param args Format arguments
	 */
	template <typename... Args>
	void debug(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (logger_) {
			logger_->log(LogLevel::Debug,
			             format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Output WARNING to logger
	 * @tparam Args Format arguments types
	 * @param fmt Format string
	 * @param args Format arguments
	 */
	template <typename... Args>
	void warning(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (logger_) {
			logger_->log(LogLevel::Warning,
			             format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Output ERROR to logger
	 * @tparam Args Format arguments types
	 * @param fmt Format string
	 * @param args Format arguments
	 */
	template <typename... Args>
	void error(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (logger_) {
			logger_->log(LogLevel::Error,
			             format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Output FATAL to logger
	 * @tparam Args Format arguments types
	 * @param fmt Format string
	 * @param args Format arguments
	 */
	template <typename... Args>
	void fatal(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (logger_) {
			logger_->log(LogLevel::Fatal,
			             format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Returns HTTP client configuration. Always valid
	 * @return HTTP client configuration
	 */
	std::shared_ptr<const ParserConfig> config() const;

	/**
	 * @brief Shared cache of compiled parser resources (CSS selector sets,
	 *        regexes, ...), keyed by resource-set type.
	 * Shared with every context derived via new_with_config / new_with_logger,
	 * so a set compiled once is reused across a parser's requests.
	 * @return The resource cache
	 */
	ResourceCache& resources() const;

	size_t alt_link() const;
	void set_alt_link(size_t alt_link);

	/**
	 * @brief Make new context for parsers with new logger
	 * @return New context with inherited properties and new logger
	 */
	RequestorContext new_with_logger(std::shared_ptr<LoggerContext> logger) const;

	/**
	 * @brief Make new context for parsers with new client
	 * @return New context with inherited properties and new client
	 */
	//RequestorContext new_with_client(std::shared_ptr<ClientContext> client) const;

	/**
	 * @brief Make new context for parsers with new HTTP client config
	 * @return New context with inherited properties and new HTTP client config
	 */
	RequestorContext new_with_config(std::shared_ptr<ParserConfig> config) const;

private:
	std::shared_ptr<ClientContext> client_;
	std::shared_ptr<LoggerContext> logger_;
	std::shared_ptr<ParserConfig> config_;
	std::shared_ptr<ResourceCache> resources_;
};
} // namespace aniparse