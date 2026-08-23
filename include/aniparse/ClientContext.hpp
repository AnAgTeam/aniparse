/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Request.hpp"
#include "aniparse/types/Response.hpp"
#include "aniparse/net/CookieJar.hpp"
#include "aniparse/catalog/MirrorSource.hpp"
#include "aniparse/cache/ResourceCache.hpp"
#include "aniparse/html/SelectorSource.hpp"
#include "aniparse/utility/RegexSource.hpp"
#include "aniparse/utility/Format.hpp"

#include "aniparse/net/CancellingTask.hpp"

#include <boost/json/fwd.hpp>

namespace aniparse::html {
class HTMLDocument;
} // namespace aniparse::html

namespace aniparse {

/// Transport-wide switches of a @ref ClientConfig. @see client_config_flags
using ClientConfigFlags = FlagsBitfield<64, struct ClientConfigFlagsTag>;
/// Per-parser request switches of a @ref ParserConfig. @see parser_config_flags
using ParserConfigFlags = FlagsBitfield<64, struct ParserConfigFlagsTag>;

/// The bits of a ClientConfigFlags.
namespace client_config_flags {
/// Validate the peer certificate and hostname on TLS connections. Clearing it
/// makes the transport accept any certificate — only meaningful for a debugging
/// proxy that terminates TLS.
inline constexpr auto verify_ssl = ClientConfigFlags::make_bit(0);
} // namespace client_config_flags

/// The bits of a ParserConfigFlags.
namespace parser_config_flags {
/// Follow HTTP redirects rather than surfacing the 3xx response. Honouring it is
/// up to the backend; the bundled curl one does not read it today, so a parser
/// cannot rely on toggling it.
inline constexpr auto follow_redirects = ParserConfigFlags::make_bit(0);
} // namespace parser_config_flags

/**
 * @brief An HTTP proxy every request of a @ref ClientContext is routed through.
 * Credentials are optional: leave @ref user and @ref passwd empty for a proxy that
 * needs no authentication.
 */
struct ClientProxy {
	std::string user;   ///< Proxy username; empty when the proxy takes no credentials.
	std::string passwd; ///< Proxy password; empty when the proxy takes no credentials.
	std::string host;   ///< Proxy hostname or address.
	uint16_t port = 0;  ///< Proxy port.
};

/**
 * @brief Parser configuration for all its requests
 *
 * The per-parser half of the configuration (the transport-wide half is
 * @ref ClientConfig): whatever a parser needs stamped onto every request it makes —
 * its default headers and url params, the cookie jar holding its session, and its
 * selected mirror. A @ref RequestorContext applies it to each outgoing request.
 *
 * A config for a given parser is not written by hand: it is *derived*, by handing a
 * base config to Parser::make_config and adopting the result via
 * RequestorContext::new_with_config. That derivation is what stamps @ref parser_id and
 * seeds the parser's defaults, and it is mandatory before any getter of that parser
 * runs — see RequestorContext::new_with_config.
 * @see RequestorContext
 */
struct ParserConfig {
	/// Default headers merged into every request. Headers already set on the request
	/// win: the config fills in only the names the request left unset.
	Headers headers;
	/// Query params appended to every request — the seam static credentials
	/// (an api key, a user id) ride on. @see Parser::authenticate_context
	std::map<std::string, std::string> url_params;
	/// The session: the jar every request of this config reads and writes cookies
	/// through. Provisioned by the context (from the client backend) when null, so a
	/// derived config that carries a restored jar keeps it.
	std::shared_ptr<CookieJar> cookie_jar;
	/// Per-parser request switches. @see parser_config_flags
	ParserConfigFlags flags;

	/// Identifier of the parser this config was derived for, stamped by
	/// Parser::make_config. Lets request-time services keyed by parser (mirror
	/// overrides) find this parser's entry without the getter naming its own id.
	/// Empty on a config that was not derived through make_config.
	std::string parser_id;

	/// Selected alternative link (mirror) index. @see AltLink, RequestorContext::alt_link
	size_t alt_link = 0;

	/// Last-mile hooks run on every outgoing request, after the headers and url params
	/// above have been applied, in order. The escape hatch for a request property that
	/// is computed rather than constant (a signature over the final URL, say).
	std::vector<std::function<void(aniparse::ClientRequest&)>> modifiers;
};

/**
 * @brief Transport-wide configuration of a @ref ClientContext.
 *
 * Applies to every request the client performs, whatever parser issued it — this is
 * the host application's knob (proxy, timeout, retry budget), as opposed to
 * @ref ParserConfig, which is per-parser and travels with the getter. An unset
 * optional leaves the backend's own default in place.
 * @see ClientContext::set_config
 */
struct ClientConfig {
	/// Proxy to route every request through; unset for a direct connection.
	std::optional<ClientProxy> proxy;
	/// How many times a request is retried after a transport failure (timeout,
	/// connection, TLS) before the error is surfaced. A cancelled request is never
	/// retried, and an HTTP status is not a transport failure, so a 4xx/5xx response
	/// is returned rather than retried. Unset = the backend's default budget.
	std::optional<uint32_t> max_retries;
	/// Deadline for a single request; unset = the backend's default.
	std::optional<std::chrono::milliseconds> timeout;
	/// Transport-wide switches. @see client_config_flags
	ClientConfigFlags flags;
};

/**
 * @brief Interface for performing HTTP requests — the backend-neutral seam the whole
 * parse core sits on.
 *
 * Parsers and getters never touch an HTTP library: they talk to a @ref RequestorContext,
 * which talks to this interface and nothing else. So the transport is swappable — the
 * implementing side is a backend (the bundled curl-based one, or a platform client
 * supplied by the host application), and the consuming side is the parse core. A
 * backend implements every method here; parser code implements none of them and calls
 * none of them directly.
 *
 * An implementation is shared (through a @ref ServiceState) by every context and every
 * parser of a session, so it must tolerate concurrent requests.
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
	 * @brief Adopt a new transport-wide configuration.
	 *
	 * Affects requests started after the call; requests already in flight keep the
	 * configuration they were started with. Not a per-parser knob — every parser
	 * sharing this client sees the change. @see ParserConfig for per-parser settings.
	 * @param config The transport-wide configuration to apply
	 */
	virtual void set_config(ClientConfig config) = 0;

	/**
	 * @brief Create an empty cookie jar this client can drive.
	 *
	 * The jar is backend-specific (it wraps whatever cookie store the transport
	 * actually reads), which is why it is minted here rather than constructed by the
	 * caller. A context provisions a jar this way for a config that has none, so a
	 * parser gets its own session automatically.
	 * @return A fresh empty jar, or nullptr for a backend that manages cookies itself
	 */
	virtual std::shared_ptr<CookieJar> make_cookie_jar() = 0;
};

/// Severity of a message handed to a @ref LoggerContext, ordered from least to most
/// severe.
enum class LogLevel {
	Debug,   ///< Diagnostic detail of interest only when tracing a parse.
	Info,    ///< Ordinary progress of an operation.
	Warning, ///< Something unexpected that the operation recovered from.
	Error,   ///< An operation failed.
	Fatal    ///< A failure the session cannot be expected to continue past.
};

/**
 * @brief Interface for logging messages
 *
 * The sink is shared by every parser of the session (it lives in @ref ServiceState),
 * so the emitter's identity travels with each message as @ref log's parser_id rather
 * than being a property of the logger — that is what lets a host filter the stream
 * per parser.
 */
struct LoggerContext {
	virtual ~LoggerContext() = default;

	/**
	 * @brief Output text with prefix and message
	 * @param message_type Prefix to message. ("INFO", "ERROR", etc.)
	 * @param message Message text to output
	 * @param parser_id Identifier of the parser the message originates from (the value
	 *        Parser::make_config stamped into the emitting context's config). Empty when
	 *        the source is not tied to a parser — a context whose config never went
	 *        through make_config.
	 * @param loc Call site the message originates from, for diagnostics
	 */
	virtual void log(LogLevel message_type,
	                 std::string_view message,
	                 std::string_view parser_id,
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
 *
 * The bundle itself is immutable once a context holds it: swapping one service out
 * (new_with_logger) copies the bundle rather than writing into the shared one, so a
 * context never observes another context's services changing under it. The services
 * live as long as any context referring to them — a context keeps the client, the
 * cache and the logger alive, and the caller need not outlive the contexts it made.
 * The services (the client above all) are used concurrently by every context derived
 * from them.
 */
struct ServiceState {
	/// Required — the context throws if this is null.
	std::shared_ptr<ClientContext> client;
	/// Optional; null = no logging.
	std::shared_ptr<LoggerContext> logger = nullptr;
	/// Optional; a fresh empty cache is filled in when null.
	std::shared_ptr<ResourceCache> resources = nullptr;
	/// Optional swappable selector-source holder; null reads as an empty source
	/// (all built-in selectors). A catalog apply set()s a new source into it.
	std::shared_ptr<html::SelectorSourceHolder> selectors = nullptr;
	/// Optional swappable regex-source holder; null reads as an empty source
	/// (all built-in pattern literals). A catalog apply set()s a new source into
	/// it, keyed by parser identifier — or by extractor identifier for a state
	/// built to feed video extractors.
	std::shared_ptr<RegexSourceHolder> patterns = nullptr;
	/// Optional swappable mirror-source holder; null reads as an empty source
	/// (every parser falls back to its built-in mirrors). A catalog apply set()s a
	/// new source into it, keyed by parser identifier. A consumer that runs video
	/// extractors keys overrides by extractor identifier instead, by building the
	/// extractor-facing contexts on a state whose holder carries the extractor
	/// mirror source.
	std::shared_ptr<MirrorSourceHolder> mirrors = nullptr;
};

/**
 * @brief General context for parsers with client, its config and logger
 *
 * What a getter is actually handed, and its only door to the outside world: it
 * carries the parser's @ref ParserConfig (default headers, url params, cookie jar,
 * selected mirror), the shared services of the session (@ref ServiceState — HTTP
 * client, logger, resource cache, selector and mirror overrides), and issues every
 * request through the @ref ClientContext beneath it, applying the config on the way
 * out. A getter therefore never names a transport, a host or a header set of its own.
 *
 * Cheap to copy (it is a pair of shared handles), and passed by value into getters.
 * Copies of a context, and contexts derived through @ref new_with_config /
 * @ref new_with_logger, share the same services and hence the same cache and client.
 *
 * The config a context carries must be one derived by Parser::make_config for the
 * parser whose getter is about to run — @see new_with_config for the contract.
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
	 *
	 * The config is applied on the way out: its headers fill in the names the request
	 * left unset (an explicit request header always wins), its url params are appended,
	 * its cookie jar carries the session, and its modifiers run last.
	 * @note Only meaningful on a context carrying a config derived by Parser::make_config
	 *       (adopted via @ref new_with_config): an underived config has none of the
	 *       parser's default headers/cookies, and sources answer such a request with a
	 *       4xx rather than data.
	 * @param request HTTP request
	 * @return Task with response
	 */
	NetworkRequestTask<ResponseData> request(GetRequest request);

	/**
	 * @brief Perform HTTP POST request with respect to client config
	 *
	 * Applies the config exactly like the GET overload, including its requirement that
	 * the config be a derived one.
	 * @param request HTTP request
	 * @return Task with response
	 */
	NetworkRequestTask<ResponseData> request(PostRequest request);

	/**
	 * @brief Perform HTTP POST multipart/form-data request with respect to client config
	 *
	 * Applies the config exactly like the GET overload, including its requirement that
	 * the config be a derived one.
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
	 * @param sourced_fmt Format string, with the call site captured implicitly
	 * @param args Format arguments
	 */
	template <typename... Args>
	void info(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (services_->logger) {
			services_->logger->log(LogLevel::Info,
			             fmt::format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             config_->parser_id,
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Output DEBUG to logger
	 * @tparam Args Format arguments types
	 * @param sourced_fmt Format string, with the call site captured implicitly
	 * @param args Format arguments
	 */
	template <typename... Args>
	void debug(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (services_->logger) {
			services_->logger->log(LogLevel::Debug,
			             fmt::format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             config_->parser_id,
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Output WARNING to logger
	 * @tparam Args Format arguments types
	 * @param sourced_fmt Format string, with the call site captured implicitly
	 * @param args Format arguments
	 */
	template <typename... Args>
	void warning(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (services_->logger) {
			services_->logger->log(LogLevel::Warning,
			             fmt::format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             config_->parser_id,
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Output ERROR to logger
	 * @tparam Args Format arguments types
	 * @param sourced_fmt Format string, with the call site captured implicitly
	 * @param args Format arguments
	 */
	template <typename... Args>
	void error(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (services_->logger) {
			services_->logger->log(LogLevel::Error,
			             fmt::format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             config_->parser_id,
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Output FATAL to logger
	 * @tparam Args Format arguments types
	 * @param sourced_fmt Format string, with the call site captured implicitly
	 * @param args Format arguments
	 */
	template <typename... Args>
	void fatal(SourcedFormatString<Args...> sourced_fmt, Args&&... args) {
		if (services_->logger) {
			services_->logger->log(LogLevel::Fatal,
			             fmt::format(sourced_fmt.fmt, std::forward<Args>(args)...),
			             config_->parser_id,
			             sourced_fmt.loc);
		}
	}

	/**
	 * @brief Returns HTTP client configuration. Always valid
	 * @return HTTP client configuration
	 */
	std::shared_ptr<const ParserConfig> config() const;

	/**
	 * @brief Return this context's shared runtime services.
	 *
	 * The returned owner keeps the HTTP client, resource cache, and volatile catalog
	 * holders alive. Hosts that construct a @ref CatalogManager for the same parser
	 * session pass this handle to it, so a successful catalog apply updates the
	 * mirrors and selectors observed by contexts derived from this one.
	 * @return The immutable shared-services handle used by this context.
	 * @note The @ref ServiceState object's holder members are intentionally mutable
	 *       synchronization points; callers must not replace the service bundle.
	 */
	[[nodiscard]] std::shared_ptr<const ServiceState> services() const noexcept {
		return services_;
	}

	/**
	 * @brief Shared cache of compiled parser resources (CSS selector sets,
	 *        regexes, ...), keyed by resource-set type.
	 * Shared with every context derived via new_with_config / new_with_logger,
	 * so a set compiled once is reused across a parser's requests.
	 * @return The resource cache
	 */
	ResourceCache& resources() const;

	/**
	 * @brief The data-driven CSS selector override source, unscoped.
	 * Empty by default (every selector falls back to its built-in literal); a
	 * populated source arrives from the volatile catalog/index. Shared across
	 * contexts derived via new_with_config / new_with_logger, like resources().
	 * This is the raw source holding every parser's table — set construction
	 * should go through @ref selectors, which scopes it to this context's parser.
	 * @return The selector source
	 */
	std::shared_ptr<const html::SelectorSource> selector_source() const;

	/**
	 * @brief Build (once per parser, cached) a selector set @p T from this
	 * context's scoped selector source, falling back to the set's built-in
	 * literals.
	 *
	 * The set is compiled against the overrides of THIS context's parser (the id
	 * Parser::make_config stamped into the config) and cached by (parser id, set
	 * type) inside the selector-source holder — so two parsers sharing one set
	 * type (an engine family) each get their own copy with their own overrides,
	 * never a neighbour's. The getter names only the set type; it names neither
	 * its id nor the override.
	 * @tparam T Selector set type exposing `static T create(const html::SelectorSource&)`
	 * @return Shared handle to the built set; hold it for the whole operation so a
	 *         concurrent catalog swap cannot free it underneath
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> selectors() const {
		if (services_ && services_->selectors) {
			return services_->selectors->set_for<T>(config_->parser_id);
		}
		// No selector holder (tests, minimal hosts): every selector falls back to
		// its built-in literal, cached by bare type in the shared resource cache.
		auto source = selector_source();
		return resources().get<T>([source] { return T::create(*source); });
	}

	/**
	 * @brief The data-driven regex pattern override source, unscoped.
	 * Empty by default (every pattern falls back to its built-in literal); a
	 * populated source arrives from the volatile catalog. Shared across contexts
	 * derived via new_with_config / new_with_logger, like resources(). This is
	 * the raw source holding every owner's table — set construction should go
	 * through @ref patterns, which scopes it to this context's parser.
	 * @return The regex pattern source
	 */
	std::shared_ptr<const RegexSource> pattern_source() const;

	/**
	 * @brief Build (once per parser, cached) a pattern set @p T from this
	 * context's scoped regex source, falling back to the set's built-in literals.
	 *
	 * The set is compiled against the overrides of THIS context's parser (the id
	 * Parser::make_config stamped into the config) and cached by (parser id, set
	 * type) inside the regex-source holder — the same discipline as
	 * @ref selectors. The getter names only the set type.
	 * @tparam T Pattern set type exposing `static T create(const RegexSource&)`
	 * @return Shared handle to the built set; hold it for the whole operation so a
	 *         concurrent catalog swap cannot free it underneath
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> patterns() const {
		return patterns<T>(config_->parser_id);
	}

	/**
	 * @brief Build a pattern set @p T scoped to an explicitly given @p id.
	 *
	 * This overload exists for video extractors (via VideoExtractor::patterns),
	 * which carry no stamped ParserConfig and so name their own stable identifier
	 * as the override scope. Parsers must call the no-argument @ref patterns()
	 * instead: their overrides are scoped by the parser identity
	 * Parser::make_config stamped into the config, and naming an id by hand can
	 * only mis-scope them.
	 * @tparam T Pattern set type exposing `static T create(const RegexSource&)`
	 * @param id The extractor's stable identifier (@see VideoExtractor::identifier).
	 * @return Shared handle to the built set; hold it for the whole operation.
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> patterns(std::string_view id) const {
		if (services_ && services_->patterns) {
			return services_->patterns->set_for<T>(id);
		}
		// No pattern holder (tests, minimal hosts): every pattern falls back to
		// its built-in literal, cached by bare type in the shared resource cache.
		auto source = pattern_source();
		return resources().get<T>([source] { return T::create(*source); });
	}

	/**
	 * @brief A mirror view for this context's parser: the catalog override for it
	 * (if any) combined with the getter's built-in @p builtin fallback.
	 *
	 * The override is looked up by the parser identity stamped into the config by
	 * Parser::make_config, against a mirror snapshot taken once when this context
	 * was built — so the whole operation reads one consistent mirror set even if
	 * the catalog swaps mid-request. The getter passes only its own built-in list;
	 * it names neither its id nor the override.
	 * @param builtin The getter's built-in fallback base URLs.
	 * @return The combined mirror view (@see Mirrors).
	 */
	[[nodiscard]] Mirrors mirrors(std::span<const std::string_view> builtin) const {
		const std::vector<std::string>* override_list =
		    mirror_snapshot_ ? mirror_snapshot_->list_for(config_->parser_id) : nullptr;
		return Mirrors{ override_list, builtin };
	}

	/**
	 * @brief A mirror view keyed by an explicitly given @p id: the catalog
	 * override for it (if any) combined with the @p builtin fallback.
	 *
	 * This overload exists for video extractors (via VideoExtractor::mirrors),
	 * which carry no stamped ParserConfig and so name their own stable
	 * identifier as the override key. Parsers must call the single-argument
	 * @ref mirrors() instead: their override is keyed by the parser identity
	 * Parser::make_config stamped into the config, and naming an id by hand can
	 * only mis-key it.
	 * @param id The extractor's stable identifier (@see VideoExtractor::identifier).
	 * @param builtin The caller's built-in fallback base URLs.
	 * @return The combined mirror view (@see Mirrors).
	 */
	[[nodiscard]] Mirrors mirrors(std::string_view id,
	                              std::span<const std::string_view> builtin) const {
		const std::vector<std::string>* override_list =
		    mirror_snapshot_ ? mirror_snapshot_->list_for(id) : nullptr;
		return Mirrors{ override_list, builtin };
	}

	/**
	 * @brief The base URL to fetch from for this context's selected mirror.
	 * Folds the override/built-in resolution and the alt_link() selection into one
	 * call — the common getter path. Resolve it once into a local at the start of
	 * an operation so a concurrent selection change cannot split it across mirrors.
	 * @param builtin The getter's built-in fallback base URLs.
	 * @return The selected base URL, or an empty view if the parser has no mirrors.
	 */
	[[nodiscard]] std::string_view base_url(std::span<const std::string_view> builtin) const {
		return mirrors(builtin).base_url(alt_link());
	}

	/**
	 * @brief The canonical frontend origin for this context's parser.
	 *
	 * Resolves the signed catalog override from the same MirrorSource snapshot as
	 * @ref mirrors, or returns @p builtin when the catalog has no replacement.
	 * Use it for public source links, web authentication, and origin-sensitive
	 * request headers; do not use it as an API fetch base unless that is the
	 * source's explicit contract.
	 * @param builtin The parser's built-in canonical frontend origin.
	 * @return The resolved canonical origin. The view remains valid while this
	 * context lives.
	 */
	[[nodiscard]] std::string_view canonical_base_url(std::string_view builtin) const {
		const std::string* override_url =
		    mirror_snapshot_ ? mirror_snapshot_->canonical_base_for(config_->parser_id) : nullptr;
		return override_url ? std::string_view(*override_url) : builtin;
	}

	/**
	 * @brief Index of the mirror currently selected for this context's parser.
	 * Indexes the mirror view @ref mirrors() resolves; 0 (the first mirror) unless a
	 * caller picked another. @see ParserConfig::alt_link
	 * @return The selected mirror index
	 */
	size_t alt_link() const;

	/**
	 * @brief Select which mirror this context's getters fetch from.
	 *
	 * Writes through to the shared config, so every context holding that config sees
	 * the new selection — this is a user-facing switch ("use this mirror"), not a
	 * per-request one. An index past the end of the mirror view resolves to an empty
	 * base URL, so a caller should pick from Parser::mirror_choices.
	 * @param alt_link Index into the resolved mirror view
	 */
	void set_alt_link(size_t alt_link);

	/**
	 * @brief Make new context for parsers with new logger
	 *
	 * Everything else is inherited: the same config and the same shared services (so
	 * the cache, client and cookie jar are still shared with the original).
	 * @param logger Logger the new context reports through; nullptr disables logging
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
	 *
	 * The adoption half of the mandatory config derivation, and the step callers most
	 * often skip. Before calling any getter of a parser, a caller must derive that
	 * parser's config and adopt it here:
	 * @code
	 * auto config  = parser.make_config(context.config());
	 * auto ready   = context.new_with_config(config);
	 * auto results = co_await parser.mangas_getter()->latest(ready, filters);
	 * @endcode
	 * make_config is what stamps the parser's identity (without which request-time
	 * services keyed by parser — a catalog's mirror overrides — cannot find the
	 * parser's entry) and seeds the parser's default headers, url params and fresh
	 * cookie jar. Handing a getter a context whose config never went through
	 * make_config is not a compile error and not an exception: the requests simply go
	 * out stripped of what the source expects and come back 4xx.
	 *
	 * Cheap: only the config handle changes, the shared services are passed on as-is.
	 * @param config Derived config to adopt; nullptr yields a default (underived) config
	 * @return New context with inherited properties and new HTTP client config
	 */
	RequestorContext new_with_config(std::shared_ptr<ParserConfig> config) const;

private:
	std::shared_ptr<const ServiceState> services_;
	std::shared_ptr<ParserConfig> config_;
	/// Mirror and canonical-origin overrides snapshotted once at construction, so
	/// an operation reads a consistent source definition even across a concurrent
	/// catalog swap. Null when the services carry no mirror holder (every parser
	/// falls back to its built-ins).
	std::shared_ptr<const MirrorSource> mirror_snapshot_;
};
} // namespace aniparse
