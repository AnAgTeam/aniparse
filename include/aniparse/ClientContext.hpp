/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Request.hpp"
#include "aniparse/types/Response.hpp"
#include "aniparse/CookieJar.hpp"
#include "aniparse/ResourceCache.hpp"
#include "aniparse/html/SelectorSource.hpp"
#include "aniparse/utility/Format.hpp"

#include <asyncnet/CancellingTask.hpp>

#include <boost/json/fwd.hpp>

namespace aniparse::html {
class HTMLDocument;
} // namespace aniparse::html

namespace aniparse {

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
	 * @return Task with the response, or a RequestError on a transport-level failure
	 *         (timeout, connection, TLS, cancellation). No status check here — the
	 *         raw HTTP status stays in ResponseData for the caller to inspect.
	 */
	virtual NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest request) = 0;

	/**
	 * @brief Perform HTTP POST request
	 * @param request HTTP request
	 * @return Task with the response, or a RequestError on a transport-level failure
	 *         (timeout, connection, TLS, cancellation). No status check here.
	 */
	virtual NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest request) = 0;

	/**
	 * @brief Perform HTTP POST multipart/form-data request
	 * @param request HTTP request
	 * @return Task with the response, or a RequestError on a transport-level failure.
	 *         No status check here.
	 * @throws std::invalid_argument If a File form part sets an explicit filename
	 *         (the transport presents the on-disk name and cannot override it) — a caller bug,
	 *         raised when the returned task is awaited
	 */
	virtual NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest request) = 0;

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
 * @brief The per-parser-invariant services a RequestorContext runs on: the HTTP
 * client, logger, compiled-resource cache and CSS selector overrides.
 *
 * Grouped into one handle so the context takes (services, config) instead of a
 * parameter list that grows with every new shared service. Held by
 * shared_ptr<const> and shared across contexts derived via new_with_config (a
 * no-alloc pointer swap) / new_with_logger. Only @ref client is required; the
 * rest default in (an empty resource cache, an empty selector source). New
 * shared services are added HERE, not as constructor parameters.
 */
struct ServiceState {
	/// Required — the context throws if this is null.
	std::shared_ptr<ClientContext> client;
	/// Optional; null = no logging.
	std::shared_ptr<LoggerContext> logger = nullptr;
	/// Optional; a fresh empty cache is filled in when null.
	std::shared_ptr<ResourceCache> resources = nullptr;
	/// Optional; null reads as SelectorSource::empty() (all built-in selectors).
	std::shared_ptr<const html::SelectorSource> selector_source = nullptr;
};

/**
 * @brief General context for parsers with client, its config and logger
 * @see ParserConfig
 */
class RequestorContext {
public:
	//constexpr RequestorContext() noexcept = default;

	/**
	 * @brief Construct a context over a bundle of shared services and a config.
	 * @param services Shared services; must be valid and carry a valid client. A
	 *        null resources is filled with a fresh cache; a null selector source
	 *        reads as empty. New shared services are added to ServiceState, not here.
	 * @param config Optional per-parser config; a null one is replaced by a default.
	 * @note Even if @p config is nullptr, config() always returns a valid config.
	 * @throws std::invalid_argument if @p services or its client is nullptr
	 */
	explicit RequestorContext(std::shared_ptr<const ServiceState> services,
	                          std::shared_ptr<ParserConfig> config = nullptr);

	/**
	 * @brief Convenience overload for the common "just a client (and maybe a
	 * logger)" case; bundles them into a ServiceState and delegates.
	 * @param client Client for HTTP requests. Should be valid
	 * @param logger Optional message logger
	 * @param config Optional per-parser config
	 * @throws std::invalid_argument if @p client is nullptr
	 */
	RequestorContext(std::shared_ptr<ClientContext> client,
	                 std::shared_ptr<LoggerContext> logger = nullptr,
	                 std::shared_ptr<ParserConfig> config = nullptr);

	/**
	 * @brief Perform HTTP GET request with respect to client config
	 * @param request HTTP request
	 * @return Task with response
	 */
	NetworkRequestTask<ResponseData> request(GetRequest request);

	/**
	 * @brief Perform HTTP POST request with respect to client config
	 * @param request HTTP request
	 * @return Task with response
	 */
	NetworkRequestTask<ResponseData> request(PostRequest request);

	/**
	 * @brief Perform HTTP POST multipart/form-data request with respect to client config
	 * @param request HTTP request
	 * @return Task with response
	 */
	NetworkRequestTask<ResponseData> request(PostMultipartRequest request);

	/**
	 * @brief Perform a GET and parse the 2xx response body as an HTML document.
	 * Non-retrying convenience over request(): checks the status, maps a non-2xx
	 * status (or a parse failure) to a RequestError, otherwise returns the parsed
	 * document. Centralizes the fetch -> status-check -> parse -> error-map path so
	 * getters don't duplicate it.
	 * @note Never throws for I/O errors: transport failures, non-2xx statuses, and
	 *       parse failures all travel through the RequestError channel.
	 * @param request HTTP GET request
	 * @return Task with the parsed document or a RequestError
	 */
	NetworkRequestTask<html::HTMLDocument> request_html(GetRequest request);

	/**
	 * @brief Perform a POST and parse the 2xx response body as an HTML document.
	 * Same contract as the GET overload: status-checks, parses, and folds any
	 * failure into the RequestError channel; never throws for I/O errors.
	 * @param request HTTP POST request
	 * @return Task with the parsed document or a RequestError
	 */
	NetworkRequestTask<html::HTMLDocument> request_html(PostRequest request);

	/**
	 * @brief Perform a GET and parse the 2xx response body as JSON.
	 * Non-retrying convenience over request(): checks the status, maps a non-2xx
	 * status (or a parse failure) to a RequestError, otherwise returns the parsed
	 * JSON value. Never throws for I/O errors.
	 * @param request HTTP GET request
	 * @return Task with the parsed JSON value or a RequestError
	 */
	NetworkRequestTask<boost::json::value> request_json(GetRequest request);

	/**
	 * @brief Perform a POST and parse the 2xx response body as JSON.
	 * Same contract as the GET overload: status-checks, parses, and folds any
	 * failure into the RequestError channel; never throws for I/O errors.
	 * @param request HTTP POST request
	 * @return Task with the parsed JSON value or a RequestError
	 */
	NetworkRequestTask<boost::json::value> request_json(PostRequest request);

	/**
	 * @brief Output INFO to logger
	 * @tparam Args Format arguments types
	 * @param fmt Format string
	 * @param args Format arguments
	 */
	template <typename... Args>
	void info(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (services_->logger) {
			services_->logger->log(LogLevel::Info,
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
		if (services_->logger) {
			services_->logger->log(LogLevel::Debug,
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
		if (services_->logger) {
			services_->logger->log(LogLevel::Warning,
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
		if (services_->logger) {
			services_->logger->log(LogLevel::Error,
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
		if (services_->logger) {
			services_->logger->log(LogLevel::Fatal,
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

	/**
	 * @brief The data-driven CSS selector overrides for this context.
	 * Empty by default (every selector falls back to its built-in literal); a
	 * populated source arrives from the volatile catalog/index. Shared across
	 * contexts derived via new_with_config / new_with_logger, like resources().
	 * @return The selector source
	 */
	const html::SelectorSource& selector_source() const;

	/**
	 * @brief Build (once, cached) a selector set @p T from this context's selector
	 * source, falling back to the set's built-in literals.
	 *
	 * Sugar over resources().get<T>(...) that feeds the selector source into
	 * T::create, so getters name the set without repeating the factory lambda.
	 * @tparam T Selector set type exposing `static T create(const html::SelectorSource&)`
	 * @return Shared handle to the built set
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> selectors() const {
		return resources().get<T>([this] { return T::create(selector_source()); });
	}

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
	std::shared_ptr<const ServiceState> services_;
	std::shared_ptr<ParserConfig> config_;
};
} // namespace aniparse