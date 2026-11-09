/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/runtime/RuntimeServices.hpp"
#include "aniparse/source/ParserConfig.hpp"
#include "aniparse/detail/ProfileBinding.hpp"
#include "aniparse/utility/Format.hpp"
#include <boost/json/fwd.hpp>
#include <stdexcept>

namespace aniparse {

namespace html { class HTMLDocument; }
class ParserProfile;
class ProfileSession;
class AuthenticationSession;
struct VideoExtractor;
class VideoExtractorStore;

/**
 * @brief An owning snapshot of one operation's request environment.
 * Created by ProfileSession::snapshot. Settings, profile identity, mirrors,
 * frontend origin, selectors, patterns and application request headers remain
 * fixed across suspension. Cookies evolve in the captured session jar.
 * Copies retain shared owners without copying configuration tables. Nested
 * operations receive the same context; independent operations take a new snapshot.
 */
class RequestContext {
public:
	/**
	 * @brief Perform HTTP GET request with respect to client config
	 *
	 * The config is applied on the way out: its headers fill in the names the request
	 * left unset, its url params are appended, and its cookie jar carries the session.
	 * Modifiers run last with this snapshot and may replace merged request values.
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
			             state_->binding->parser_id,
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
			             state_->binding->parser_id,
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
			             state_->binding->parser_id,
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
			             state_->binding->parser_id,
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
			             state_->binding->parser_id,
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
	 * The returned owner keeps the HTTP client, resource cache, logger, and
	 * overrides store alive. A @ref SourceOverridesManager using these services
	 * publishes new generations for subsequent session snapshots; this context
	 * retains the generation it captured.
	 * @return The immutable shared-services handle used by this context.
	 * @note Services synchronize their own mutable state. Callers must not replace
	 *       members of the service bundle after sharing it.
	 */
	[[nodiscard]] std::shared_ptr<const RuntimeServices> services() const noexcept {
		return services_;
	}

	/**
	 * @brief Shared cache of compiled parser resources (CSS selector sets,
	 *        regexes, ...), keyed by resource-set type.
	 * Shared by sessions using the same runtime services,
	 * so a set compiled once is reused across a parser's requests.
	 * @return The resource cache
	 */
	ResourceCache& resources() const;

	/**
	 * @brief Build (once per parser, cached) a selector set @p T from this
	 * context's scoped selector source, falling back to the set's built-in
	 * literals.
	 *
	 * The set is compiled against the overrides of THIS context's parser (the id
	 * ParserProfile::make_session binds privately) and cached by (parser id, set
	 * type) inside this overrides generation — so two parsers sharing one set
	 * type (an engine family) each get their own copy with their own overrides,
	 * never a neighbour's. The getter names only the set type; it names neither
	 * its id nor the override.
	 * @tparam T Selector set type exposing `static T make(const html::SelectorSource&)`
	 * @return Shared handle to the built set; hold it for the whole operation so a
	 *         concurrent catalog swap cannot free it underneath
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> selectors() const {
        return overrides_->selectors<T>(state_->binding->parser_id);
	}

	/**
	 * @brief Build (once per parser, cached) a pattern set @p T from this
	 * context's scoped regex source, falling back to the set's built-in literals.
	 *
	 * The set is compiled against the overrides of THIS context's parser (the id
	 * ParserProfile::make_session binds privately) and cached by (parser id, set
	 * type) inside this overrides generation — the same discipline as
	 * @ref selectors. The getter names only the set type.
	 * @tparam T Pattern set type exposing `static T make(const RegexSource&)`
	 * @return Shared handle to the built set; hold it for the whole operation so a
	 *         concurrent catalog swap cannot free it underneath
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> patterns() const {
		return patterns<T>(state_->binding->parser_id);
	}

    /**
     * @brief The fetch base resolved once for this operation.
     * @return A view retained by this context; empty if the profile has no base.
     */
    [[nodiscard]] std::string_view base_url() const noexcept { return base_url_; }

    /**
     * @brief The independent frontend origin resolved for this operation.
     * @return A view retained by this context; empty if none is declared.
     */
    [[nodiscard]] std::string_view canonical_base_url() const noexcept { return canonical_base_url_; }

private:
    [[nodiscard]] Mirrors mirrors() const;

	template <class T>
	[[nodiscard]] std::shared_ptr<const T> patterns(std::string_view id) const {
        return overrides_->patterns<T>(id);
	}


	[[nodiscard]] Mirrors mirrors(std::string_view id,
	                              std::span<const std::string_view> builtin) const {
		const std::vector<std::string>* override_list =
		    overrides_->mirrors ? overrides_->mirrors->list_for(id) : nullptr;
		return Mirrors{ override_list, builtin };
	}

    friend class ParserProfile;
    friend class ProfileSession;
    friend class AuthenticationSession;
    friend struct VideoExtractor;
    friend class VideoExtractorStore;

    RequestContext(std::shared_ptr<const RuntimeServices> services,
                   std::shared_ptr<const detail::SessionState> state,
                   std::shared_ptr<const OverridesState> overrides,
                   std::shared_ptr<const RequestDefaults> defaults);
    static RequestContext make_extractor(std::shared_ptr<const RuntimeServices> services,
                                         std::string extractor_id = {});
    static std::shared_ptr<const OverridesState> capture_overrides(const RuntimeServices& services);
    void resolve_urls();

    std::shared_ptr<const RuntimeServices> services_;
    std::shared_ptr<const detail::SessionState> state_;
    std::shared_ptr<const OverridesState> overrides_;
    std::shared_ptr<const RequestDefaults> defaults_;
    std::string_view base_url_;
    std::string_view canonical_base_url_;
};

} // namespace aniparse
