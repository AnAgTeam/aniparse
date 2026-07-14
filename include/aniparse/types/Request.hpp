/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/FlagsBitfield.hpp"
#include "aniparse/types/Headers.hpp"
#include "aniparse/utility/UrlEncode.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/**
 * @file
 * What a parser hands to the network layer: the request shapes (GET, POST,
 * multipart), editable query parameters, and the ConfiguredRequest that pairs a
 * request with the parser config it runs under. All of it is backend-neutral —
 * nothing here names curl — which is what lets the HTTP backend be swapped
 * without touching a single parser.
 */

namespace aniparse {
/// The per-call option set carried by a @ref aniparse::GetterContext. A strong flag type:
/// its bits are the ones named in @ref aniparse::getter_flags and cannot be mixed
/// with any other flag bitfield.
using GetterContextFlags = FlagsBitfield<64, struct GetterContextFlagsTag>;

/// The bits of @ref aniparse::GetterContextFlags.
namespace getter_flags {
/// Reserved bit: the caller declares that partially filled item info is acceptable.
/// @warning Nothing in the library reads this bit today — no getter takes a
/// @ref aniparse::GetterContext — so setting it currently changes no behavior. Its
/// exact contract is fixed only once a getter consumes it.
constexpr auto partial_info_flag = GetterContextFlags::make_bit(0);

/// No flags set: the plain "complete result, no options" request.
constexpr GetterContextFlags default_flags;
} // namespace getter_flags

/**
 * @brief Editable URL query parameters, kept as ordered raw (unencoded) pairs.
 * Unlike a pre-serialized query string, individual parameters can be edited or
 * removed after the fact. Duplicate keys are allowed (?tag=a&tag=b); use add()
 * to append and set() to overwrite. Percent-encoding is applied by the
 * transport backend when the request is issued, so values are stored verbatim.
 */
class UrlParameters {
public:
	/// One query parameter: a raw (unencoded) key and its raw value.
	using value_type = std::pair<std::string, std::string>;

	/// An empty parameter set.
	UrlParameters() = default;
	/**
	 * @brief Build from a brace list of raw pairs, in the order written; repeated
	 * keys are kept as written, exactly as @ref add would.
	 * @param init Raw key/value pairs
	 */
	UrlParameters(std::initializer_list<value_type> init) : params_(init) {}

	/**
	 * @brief Overwrite the first pair whose key matches, or append if none does.
	 * The "one value per key" edit: later pairs with the same key, if any were
	 * added, are left untouched.
	 * @param key Raw parameter name
	 * @param value Raw parameter value
	 * @return *this, so calls chain
	 */
	UrlParameters& set(std::string key, std::string value) {
		if (auto it = find(key); it != params_.end()) {
			it->second = std::move(value);
		} else {
			params_.emplace_back(std::move(key), std::move(value));
		}
		return *this;
	}

	/**
	 * @brief Append a pair unconditionally, allowing repeated keys.
	 * The multi-value form (?tag=a&tag=b): a source that reads a key more than
	 * once needs each occurrence to survive, which @ref set would collapse.
	 * @param key Raw parameter name
	 * @param value Raw parameter value
	 * @return *this, so calls chain
	 */
	UrlParameters& add(std::string key, std::string value) {
		params_.emplace_back(std::move(key), std::move(value));
		return *this;
	}

	/**
	 * @brief Remove every pair whose key matches — all occurrences, not just the first.
	 * @param key Raw parameter name
	 * @return Number of pairs removed
	 */
	std::size_t remove(std::string_view key) {
		const auto before = params_.size();
		std::erase_if(params_, [&](const value_type& p) { return p.first == key; });
		return before - params_.size();
	}

	/**
	 * @brief Look up a key's value. With repeated keys only the first is reachable
	 * this way; iterate to read them all.
	 * @param key Raw parameter name
	 * @return First value stored for @p key, or nullopt if absent. Borrows into this
	 *         object: it dangles once the parameter is overwritten, removed, or the
	 *         set is destroyed.
	 */
	[[nodiscard]] std::optional<std::string_view> get(std::string_view key) const {
		if (auto it = find(key); it != params_.end()) {
			return std::string_view(it->second);
		}
		return std::nullopt;
	}

	/**
	 * @brief Whether at least one pair carries @p key.
	 * @param key Raw parameter name
	 * @return true if the key occurs at least once
	 */
	[[nodiscard]] bool contains(std::string_view key) const { return find(key) != params_.end(); }
	/// @return true if no parameter is stored.
	[[nodiscard]] bool empty() const noexcept { return params_.empty(); }
	/// @return Number of stored pairs — occurrences, not distinct keys.
	[[nodiscard]] std::size_t size() const noexcept { return params_.size(); }
	/// Drop every parameter.
	void clear() noexcept { params_.clear(); }

	/// @return Iterator to the first pair, in insertion order.
	[[nodiscard]] auto begin() const noexcept { return params_.begin(); }
	/// @return Iterator past the last pair.
	[[nodiscard]] auto end() const noexcept { return params_.end(); }

	/// @return The underlying ordered pairs, for transport serialization.
	[[nodiscard]] const std::vector<value_type>& pairs() const noexcept { return params_; }

	/// Equal when both sets hold the same raw pairs in the same order — order and
	/// duplicates are part of the value, since both are part of the query sent.
	friend bool operator==(const UrlParameters& left, const UrlParameters& right) = default;

private:
	[[nodiscard]] std::vector<value_type>::const_iterator find(std::string_view key) const {
		return std::find_if(params_.begin(), params_.end(),
		                    [&](const value_type& p) { return p.first == key; });
	}
	[[nodiscard]] std::vector<value_type>::iterator find(std::string_view key) {
		return std::find_if(params_.begin(), params_.end(),
		                    [&](const value_type& p) { return p.first == key; });
	}

	std::vector<value_type> params_;
};

/**
 * @brief Serialize parameters into an application/x-www-form-urlencoded body
 * ("k=v&k=v"), percent-encoding every key and value.
 * Build the form by pushing raw (unencoded) pairs into a @ref aniparse::UrlParameters,
 * then hand the result to a POST body — the encoding happens here, so callers
 * never escape by hand. @see url_encode
 * @param params Raw key/value pairs
 * @return Encoded form body
 */
inline std::string to_urlencoded(const UrlParameters& params) {
	std::string body;
	for (const auto& [key, value] : params) {
		if (!body.empty()) {
			body.push_back('&');
		}
		body += url_encode(key);
		body.push_back('=');
		body += url_encode(value);
	}
	return body;
}

/**
 * @brief One part of a multipart/form-data body.
 * The @ref source variant selects where the part's payload comes from; the
 * transport backend maps each alternative to its native form primitive.
 * @ref filename and @ref content_type are optional cross-cutting attributes: a
 * File source presents its own on-disk name, while a Buffer source carries no
 * inherent name, so set one here when the server needs it.
 */
struct MultipartPart {
	/// Plain text field: the value is sent inline as the part body.
	struct Text {
		/// The field's literal value, sent as the part body.
		std::string value;
		/// Equal when the values match.
		friend bool operator==(const Text&, const Text&) = default;
	};
	/// File whose bytes are held in a caller-provided in-memory buffer.
	struct Buffer {
		/// The raw bytes of the part; binary-safe (not a text field, not NUL-terminated).
		/// Moved into the transport when the request is issued.
		std::string data;
		/// Equal when the byte sequences match.
		friend bool operator==(const Buffer&, const Buffer&) = default;
	};
	/// File read from a filesystem path by the transport, which streams it lazily
	/// so a large on-disk file is not loaded into memory.
	struct File {
		/// Filesystem path the transport opens at send time; it must still exist and
		/// be readable then, since nothing is copied when the part is built.
		std::string path;
		/// Equal when the paths match — a path comparison, not a content comparison.
		friend bool operator==(const File&, const File&) = default;
	};

	/// Form field name, as the server expects it in the Content-Disposition of the part.
	std::string name;
	/// Where this part's payload comes from, and thus which form primitive the
	/// transport builds: an inline field (Text), in-memory bytes (Buffer), or a
	/// path streamed off disk (File).
	std::variant<Text, Buffer, File> source;
	/// Filename presented to the server. Meaningful for a Buffer, which has no name
	/// of its own; nullopt sends the part without a filename. A File presents its
	/// on-disk name and cannot override it — setting this on a File part is a caller
	/// error, reported by the request (@see ClientContext::do_request); build a
	/// Buffer part when a custom name is required.
	std::optional<std::string> filename;
	/// MIME type of the part (e.g. "image/png"); nullopt leaves the choice to the transport.
	std::optional<std::string> content_type;

	/// Equal when name, payload source, filename and content type all match.
	friend bool operator==(const MultipartPart&, const MultipartPart&) = default;
};

/// An ordered multipart/form-data body.
using MultipartForm = std::vector<MultipartPart>;

/**
 * @brief HTTP request method (verb).
 * The request struct fixes the body shape; this selects the verb layered on top.
 * Defaults match the struct (GetRequest -> Get, PostRequest / PostMultipartRequest
 * -> Post), so the common case sets nothing. Head suppresses the response body;
 * Put/Patch/Delete reuse the struct's body. A backend that cannot express a verb
 * reports RequestErrorCode::NotImplemented rather than silently downgrading it.
 */
enum class HttpMethod {
	Get,     ///< Retrieve the resource; the default of @ref GetRequest.
	Head,    ///< Like Get, but the server sends headers only — no response body.
	Post,    ///< Submit the request body; the default of @ref PostRequest and @ref PostMultipartRequest.
	Put,     ///< Replace the resource with the request body.
	Patch,   ///< Apply a partial modification carried in the request body.
	Delete,  ///< Remove the resource.
	Options, ///< Ask the server which capabilities the resource supports.
};

/**
 * @brief A bodyless HTTP request: the shape a getter fills to fetch a page or an
 * API resource. The plain-value input of RequestorContext::request; the parser
 * config's headers and parameters are folded in at that point, so a getter states
 * only what is specific to this one request.
 */
struct GetRequest {
	/// Absolute request URL without a query string — query goes into @ref url_params.
	/// A getter builds it from the context's selected base URL, never from a hardcoded host.
	std::string url;
	/// Query parameters, raw and unencoded; percent-encoded by the transport when
	/// the request is issued. The parser config's own parameters are appended to
	/// these before sending.
	UrlParameters url_params;
	/// Headers specific to this request. Merged with the parser config's headers when
	/// the request is issued; on a name collision the value set here wins.
	Headers headers;
	/// The verb to send. Defaults to Get; set it to Head/Delete/Options to reuse this
	/// bodyless shape with another verb.
	HttpMethod method = HttpMethod::Get;

	/// Equal when url, parameters, headers and verb all match.
	friend bool operator==(const GetRequest& left, const GetRequest& right) = default;
};

/**
 * @brief An HTTP request with a single opaque body: the shape for a JSON API call,
 * a GraphQL query, or a urlencoded form (@see to_urlencoded). The body is sent
 * verbatim, so the Content-Type describing it belongs in @ref headers (or in the
 * parser config, when every request of that parser uses the same one).
 */
struct PostRequest {
	/// Absolute request URL without a query string — query goes into @ref url_params.
	std::string url;
	/// Query parameters, raw and unencoded; some sources want parameters in the query
	/// even on a POST. Encoded by the transport; the config's parameters are appended.
	UrlParameters url_params;
	/// Headers specific to this request, merged over the parser config's headers
	/// (this one wins on a collision).
	Headers headers;
	/// The request body, sent as-is; no encoding is applied. Its format is whatever
	/// the Content-Type header announces.
	std::string body;
	/// The verb to send. Defaults to Post; set it to Put/Patch to reuse this
	/// body-carrying shape with another verb.
	HttpMethod method = HttpMethod::Post;

	/// Equal when url, parameters, headers, body and verb all match.
	friend bool operator==(const PostRequest& left, const PostRequest& right) = default;
};

/**
 * @brief An HTTP request whose body is a multipart/form-data form: the upload
 * shape. The transport builds the body from the parts and sets the boundary and
 * Content-Type itself, which is why the form is passed structurally instead of as
 * a pre-serialized body.
 */
struct PostMultipartRequest {
	/// Absolute request URL without a query string — query goes into @ref url_params.
	std::string url;
	/// Query parameters, raw and unencoded; encoded by the transport, with the parser
	/// config's parameters appended.
	UrlParameters url_params;
	/// Headers specific to this request, merged over the parser config's headers
	/// (this one wins on a collision). The multipart Content-Type is not set here —
	/// the transport writes it, boundary included.
	Headers headers;
	/// The form parts, in the order they go on the wire. @see MultipartPart
	MultipartForm forms;
	/// The verb to send. Defaults to Post; set it to Put/Patch to send the same form
	/// under another verb.
	HttpMethod method = HttpMethod::Post;

	/// Equal when url, parameters, headers, form parts and verb all match.
	friend bool operator==(const PostMultipartRequest& left, const PostMultipartRequest& right) = default;
};

/**
 * HTTP requests, that is supported by aniparse clients.
 * Used by parser getters to virtually create any HTTP requests.
 * @see GetRequest, @see PostRequest, @see PostMultipartRequest
 */
using ClientRequest = std::variant<
    GetRequest,
    PostRequest,
    PostMultipartRequest>;

/**
 * @brief Caller-supplied options for one getter call: free-form arguments plus a
 * flag set, kept separate from the request itself so an option can be added
 * without touching a getter's signature.
 * @warning Currently unused: no getter in the library takes a GetterContext, so
 * nothing reads either member yet. @see GetterContextFlags
 */
struct GetterContext {
	/// Arbitrary key/value arguments from the caller, opaque to the library; a getter
	/// that needs a non-standard knob agrees on a key with its consumer.
	std::map<std::string, std::string> user_args;

	/// The option bits for this call. @see aniparse::getter_flags
	GetterContextFlags flags = getter_flags::default_flags;
};

/**
 * @brief A request as it leaves the parser layer for the transport: the raw
 * request plus the session it runs under.
 *
 * "Configured" means RequestorContext::request has already stamped the parser's
 * ParserConfig onto the raw request — the config's headers merged in (the
 * request's own values winning on a name collision), the config's URL parameters
 * appended, and the config's request modifiers applied — and attached the cookie
 * jar of that parser's session. A ClientContext therefore receives a request that
 * is complete: it adds nothing parser-specific, which is what keeps the transport
 * backend interchangeable. Constructing one by hand bypasses that derivation and
 * is not the parser-facing path.
 * @tparam T The raw request shape being carried: @ref GetRequest, @ref PostRequest
 *         or @ref PostMultipartRequest.
 */
template <typename T>
struct ConfiguredRequest {
	/// The request to send, with the parser config already folded in.
	T request;
	/// The cookie jar of the parser's session — read for the cookies to send and
	/// updated with the ones the response sets. Shared across every request of that
	/// parser, which is what makes a login persist. May be null if the transport
	/// backend provides no jar.
	std::shared_ptr<class CookieJar> cookies;
};

/// A @ref aniparse::GetRequest ready for the transport. @see ConfiguredRequest
using ConfiguredGetRequest           = ConfiguredRequest<GetRequest>;
/// A @ref aniparse::PostRequest ready for the transport. @see ConfiguredRequest
using ConfiguredPostRequest          = ConfiguredRequest<PostRequest>;
/// A @ref aniparse::PostMultipartRequest ready for the transport. @see ConfiguredRequest
using ConfiguredPostMultipartRequest = ConfiguredRequest<PostMultipartRequest>;
} // namespace aniparse