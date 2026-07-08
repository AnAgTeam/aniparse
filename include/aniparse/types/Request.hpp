/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"
#include "aniparse/Headers.hpp"
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

namespace aniparse {
using GetterContextFlags = FlagsBitfield<64, struct GetterContextFlagsTag>;

namespace getter_flags {
constexpr auto partial_info_flag = GetterContextFlags::make_bit(0);

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
	using value_type = std::pair<std::string, std::string>;

	UrlParameters() = default;
	UrlParameters(std::initializer_list<value_type> init) : params_(init) {}

	/// Overwrite the first pair whose key matches, or append if none does.
	UrlParameters& set(std::string key, std::string value) {
		if (auto it = find(key); it != params_.end()) {
			it->second = std::move(value);
		} else {
			params_.emplace_back(std::move(key), std::move(value));
		}
		return *this;
	}

	/// Append a pair unconditionally, allowing repeated keys.
	UrlParameters& add(std::string key, std::string value) {
		params_.emplace_back(std::move(key), std::move(value));
		return *this;
	}

	/// Remove every pair whose key matches. @return Number of pairs removed.
	std::size_t remove(std::string_view key) {
		const auto before = params_.size();
		std::erase_if(params_, [&](const value_type& p) { return p.first == key; });
		return before - params_.size();
	}

	/// @return First value stored for @p key, or nullopt if absent.
	[[nodiscard]] std::optional<std::string_view> get(std::string_view key) const {
		if (auto it = find(key); it != params_.end()) {
			return std::string_view(it->second);
		}
		return std::nullopt;
	}

	[[nodiscard]] bool contains(std::string_view key) const { return find(key) != params_.end(); }
	[[nodiscard]] bool empty() const noexcept { return params_.empty(); }
	[[nodiscard]] std::size_t size() const noexcept { return params_.size(); }
	void clear() noexcept { params_.clear(); }

	[[nodiscard]] auto begin() const noexcept { return params_.begin(); }
	[[nodiscard]] auto end() const noexcept { return params_.end(); }

	/// @return The underlying ordered pairs, for transport serialization.
	[[nodiscard]] const std::vector<value_type>& pairs() const noexcept { return params_; }

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
 * Build the form by pushing raw (unencoded) pairs into a @ref UrlParameters,
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
 * @ref filename and @ref content_type are optional cross-cutting attributes:
 * for a File source an empty filename defaults to the path's own name, a set
 * one overrides it; a Buffer source carries no inherent name, so set it here
 * when the server needs one.
 */
struct MultipartPart {
	/// Plain text field: the value is sent inline as the part body.
	struct Text {
		std::string value;
		friend bool operator==(const Text&, const Text&) = default;
	};
	/// File whose bytes are held in a caller-provided in-memory buffer.
	struct Buffer {
		std::string data;
		friend bool operator==(const Buffer&, const Buffer&) = default;
	};
	/// File read from a filesystem path by the transport, which streams it lazily
	/// so a large on-disk file is not loaded into memory.
	struct File {
		std::string path;
		friend bool operator==(const File&, const File&) = default;
	};

	/// Form field name.
	std::string name;
	/// Payload source for this part.
	std::variant<Text, Buffer, File> source;
	/// Filename presented to the server. Empty => derive from a File path, or
	/// send no filename for a Buffer.
	std::optional<std::string> filename;
	/// MIME type override (e.g. "image/png"). Empty => let the transport decide.
	std::optional<std::string> content_type;

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
	Get,
	Head,
	Post,
	Put,
	Patch,
	Delete,
	Options,
};

/**
 * HTTP GET request
 */
struct GetRequest {
	std::string url;
	UrlParameters url_params;
	Headers headers;
	HttpMethod method = HttpMethod::Get;

	friend bool operator==(const GetRequest& left, const GetRequest& right) = default;
};

/**
 * HTTP POST request
 */
struct PostRequest {
	std::string url;
	UrlParameters url_params;
	Headers headers;
	std::string body;
	HttpMethod method = HttpMethod::Post;

	friend bool operator==(const PostRequest& left, const PostRequest& right) = default;
};

/**
 * HTTP POST multipart/form-data request
 */
struct PostMultipartRequest {
	std::string url;
	UrlParameters url_params;
	Headers headers;
	MultipartForm forms;
	HttpMethod method = HttpMethod::Post;

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

struct GetterContext {
	std::map<std::string, std::string> user_args;

	GetterContextFlags flags = getter_flags::default_flags;
};

template <typename T>
struct ConfiguredRequest {
	T request;
	std::shared_ptr<class CookieJar> cookies;
};

using ConfiguredGetRequest           = ConfiguredRequest<GetRequest>;
using ConfiguredPostRequest          = ConfiguredRequest<PostRequest>;
using ConfiguredPostMultipartRequest = ConfiguredRequest<PostMultipartRequest>;
} // namespace aniparse