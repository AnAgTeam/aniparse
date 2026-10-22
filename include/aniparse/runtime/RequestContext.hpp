/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/runtime/RuntimeServices.hpp"
#include "aniparse/source/ParserConfig.hpp"
#include "aniparse/utility/Format.hpp"
#include <boost/json/fwd.hpp>
#include <stdexcept>

namespace aniparse {

namespace html { class HTMLDocument; }
class ParserProfile;
struct VideoExtractor;
class VideoExtractorStore;

/**
 * @brief General context for parsers with client, its config and logger
 *
 * What a getter is actually handed, and its only door to the outside world: it
 * carries the parser's @ref ParserConfig (default headers, url params, cookie jar,
 * selected mirror), the shared services of the session (@ref RuntimeServices — HTTP
 * client, logger, resource cache, selector and mirror overrides), and issues every
 * request through the @ref HttpClient beneath it, applying the config on the way
 * out. A getter therefore never names a transport, a host or a header set of its own.
 *
 * Cheap to copy (it is a pair of shared handles), and passed by value into getters.
 * Copies of a context, and contexts derived through @ref new_with_config /
 * @ref new_with_logger, share the same services and hence the same cache and client.
 *
 * Create parser contexts through ParserProfile::make_context. The parser/profile
 * binding is private and survives configuration replacement. Extractor contexts
 * are created through VideoExtractor or VideoExtractorStore instead.
 * @see ParserConfig
 */
class RequestContext {
public:
	/**
	 * @brief Perform HTTP GET request with respect to client config
	 *
	 * The config is applied on the way out: its headers fill in the names the request
	 * left unset (an explicit request header always wins), its url params are appended,
	 * its cookie jar carries the session, and its modifiers run last.
	 * @param request HTTP request
	 * @return Task with response
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<HttpResponse> request(GetRequest request);

	/**
	 * @brief Perform HTTP POST request with respect to client config
	 *
	 * Applies the config exactly like the GET overload.
	 * @param request HTTP request
	 * @return Task with response
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<HttpResponse> request(PostRequest request);

	/**
	 * @brief Perform HTTP POST multipart/form-data request with respect to client config
	 *
	 * Applies the config exactly like the GET overload.
	 * @param request HTTP request
	 * @return Task with response
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<HttpResponse> request(PostMultipartRequest request);

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
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<html::HTMLDocument> request_html(GetRequest request);

	/**
	 * @brief Perform a POST and parse the 2xx response body as an HTML document.
	 * Same contract as the GET overload: status-checks, parses, and folds any
	 * failure into the RequestError channel; never throws for I/O errors.
	 * @param request HTTP POST request
	 * @return Task with the parsed document or a RequestError
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<html::HTMLDocument> request_html(PostRequest request);

	/**
	 * @brief Perform a GET and parse the 2xx response body as JSON.
	 * Non-retrying convenience over request(): checks the status, maps a non-2xx
	 * status (or a parse failure) to a RequestError, otherwise returns the parsed
	 * JSON value. Never throws for I/O errors.
	 * @param request HTTP GET request
	 * @return Task with the parsed JSON value or a RequestError
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<boost::json::value> request_json(GetRequest request);

	/**
	 * @brief Perform a POST and parse the 2xx response body as JSON.
	 * Same contract as the GET overload: status-checks, parses, and folds any
	 * failure into the RequestError channel; never throws for I/O errors.
	 * @param request HTTP POST request
	 * @return Task with the parsed JSON value or a RequestError
	 * @note Keep this object alive until the returned task completes or is destroyed;
	 *       the task does not retain its receiver.
	 */
	RequestTask<boost::json::value> request_json(PostRequest request);

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
	 * @brief Read this context's profile settings without exposing its identity.
	 * @return A non-null shared view retaining the settings and cookie jar.
	 */
	std::shared_ptr<const ParserConfig> config() const;

	/**
	 * @brief Return this context's shared runtime services.
	 *
	 * The returned owner keeps the HTTP client, resource cache, and volatile catalog
	 * holders alive. Hosts that construct a @ref SourceOverridesManager for the same parser
	 * session pass this handle to it, so a successful catalog apply updates the
	 * mirrors and selectors observed by contexts derived from this one.
	 * @return The immutable shared-services handle used by this context.
	 * @note The @ref RuntimeServices object's holder members are intentionally mutable
	 *       synchronization points; callers must not replace the service bundle.
	 */
	[[nodiscard]] std::shared_ptr<const RuntimeServices> services() const noexcept {
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
	 * ParserProfile::make_context binds privately) and cached by (parser id, set
	 * type) inside the selector-source holder — so two parsers sharing one set
	 * type (an engine family) each get their own copy with their own overrides,
	 * never a neighbour's. The getter names only the set type; it names neither
	 * its id nor the override.
	 * @tparam T Selector set type exposing `static T make(const html::SelectorSource&)`
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
		return resources().get<T>([source] { return T::make(*source); });
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
	 * ParserProfile::make_context binds privately) and cached by (parser id, set
	 * type) inside the regex-source holder — the same discipline as
	 * @ref selectors. The getter names only the set type.
	 * @tparam T Pattern set type exposing `static T make(const RegexSource&)`
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
	 * which name their own stable identifier
	 * as the override scope. Parsers must call the no-argument @ref patterns()
	 * instead: their overrides are scoped by the parser identity
	 * ParserProfile::make_context binds privately, and naming an id by hand can
	 * only mis-scope them.
	 * @tparam T Pattern set type exposing `static T make(const RegexSource&)`
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
		return resources().get<T>([source] { return T::make(*source); });
	}

	/**
	 * @brief A mirror view for this context's parser: the catalog override for it
	 * (if any) combined with the getter's built-in @p builtin fallback.
	 *
	 * The override is looked up by the private parser/profile binding established by
	 * ParserProfile::make_context, against a mirror snapshot taken once when this context
	 * was built — so the whole operation reads one consistent mirror set even if
	 * the catalog swaps mid-request. The getter passes only its own built-in list;
	 * it names neither its id nor the override.
	 * @param builtin The getter's built-in fallback base URLs.
	 * @return The combined mirror view (@see Mirrors).
	 */
	[[nodiscard]] Mirrors mirrors(std::span<const std::string_view> builtin) const {
		const std::vector<std::string>* override_list =
		    mirror_snapshot_ ? mirror_snapshot_->list_for(config_->parser_id, config_->profile_id) : nullptr;
		return Mirrors{ override_list, builtin };
	}

	/**
	 * @brief A mirror view keyed by an explicitly given @p id: the catalog
	 * override for it (if any) combined with the @p builtin fallback.
	 *
	 * This overload exists for video extractors (via VideoExtractor::mirrors),
	 * which name their own stable
	 * identifier as the override key. Parsers must call the single-argument
	 * @ref mirrors() instead: their override is keyed by the parser identity
	 * ParserProfile::make_context binds privately, and naming an id by hand can
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
		    mirror_snapshot_ ? mirror_snapshot_->canonical_base_for(config_->parser_id, config_->profile_id) : nullptr;
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
	 * per-request one. An index past the end of the mirror view selects the first
	 * mirror; the base URL is empty only when no mirrors are available. A caller
	 * should pick from ParserProfile::mirror_choices.
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
	RequestContext new_with_logger(std::shared_ptr<Logger> logger) const;

	/**
	 * @brief Create a context with replacement settings and the same profile binding.
	 * @param config Settings copied into independent storage; must not be null.
	 *        An existing cookie jar is retained, or a fresh backend jar is created.
	 * @return A context sharing services and the current mirror snapshot. Later
	 *         edits to the supplied settings do not modify the returned context.
	 * @throws std::invalid_argument If config is null.
	 */
	RequestContext new_with_config(std::shared_ptr<const ParserConfig> config) const;

private:
	friend class ParserProfile;
	friend struct VideoExtractor;
	friend class VideoExtractorStore;

	// Settings and their private binding share one allocation and ownership handle.
	struct ProfileState : ParserConfig {
		std::string parser_id;
		std::string profile_id;
	};

	RequestContext(std::shared_ptr<const RuntimeServices> services,
	               std::shared_ptr<ProfileState> config);
	static RequestContext make(std::shared_ptr<const RuntimeServices> services,
	                             ParserConfig config, std::string parser_id,
	                             std::string profile_id);

	std::shared_ptr<const RuntimeServices> services_;
	std::shared_ptr<ProfileState> config_;
	/// Mirror and canonical-origin overrides snapshotted once at construction, so
	/// an operation reads a consistent source definition even across a concurrent
	/// catalog swap. Null when the services carry no mirror holder (every parser
	/// falls back to its built-ins).
	std::shared_ptr<const MirrorSource> mirror_snapshot_;
};

} // namespace aniparse
