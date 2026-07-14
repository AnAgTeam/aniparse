/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"

// Only for the convenience default upstream (the real network = curl). Without
// the curl backend, an upstream ClientContext must be injected explicitly, so
// this decorator itself pulls in no HTTP backend.
#if defined(ANIPARSE_CURL_BACKEND) && ANIPARSE_CURL_BACKEND
#include "aniparse/net/Client.hpp"
#endif

#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace aniparse::testing {

/**
 * @brief A ClientContext for examples and tests that caches responses on disk.
 *
 * A request whose (method-shaped) URL + params + body already has a file under
 * @c cache_dir is served straight from that file; otherwise the request is
 * forwarded to an upstream client (the real network by default) and a 2xx body
 * is written back for next time. This lets a parser be developed against stable,
 * inspectable response files without repeatedly hitting the source (and its rate
 * limits). Cached responses are replayed as status 200 (only 2xx bodies are
 * stored), so it is a development aid, not a faithful transport recorder.
 */
class FileCachingClient : public ClientContext {
public:
	FileCachingClient(std::filesystem::path cache_dir, std::shared_ptr<ClientContext> upstream)
	    : cache_dir_(std::move(cache_dir)), upstream_(std::move(upstream)) {
		std::filesystem::create_directories(cache_dir_);
	}

#if defined(ANIPARSE_CURL_BACKEND) && ANIPARSE_CURL_BACKEND
	// Convenience: default the upstream to the real network (curl backend).
	explicit FileCachingClient(std::filesystem::path cache_dir)
	    : FileCachingClient(std::move(cache_dir), std::make_shared<AsyncClient>()) {}
#endif

	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest request) override {
		// Compute the key BEFORE moving the request: argument evaluation order is
		// unspecified, so name it in a sequenced statement to avoid reading a
		// moved-from url.
		std::string name = cache_name(request.request.url, request.request.url_params, {});
		co_return co_await serve(std::move(name), std::move(request));
	}

	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest request) override {
		std::string name = cache_name(request.request.url, request.request.url_params, request.request.body);
		co_return co_await serve(std::move(name), std::move(request));
	}

	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest request) override {
		// Multipart bodies are not keyed; always go upstream.
		co_return co_await upstream_->do_request(std::move(request));
	}

	void set_config(ClientConfig config) override {
		upstream_->set_config(std::move(config));
	}

	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return upstream_->make_cookie_jar();
	}

private:
	template <typename Configured>
	NetworkRequestTask<ResponseData> serve(std::string name, Configured request) {
		const std::filesystem::path file = cache_dir_ / name;
		if (std::filesystem::exists(file)) {
			co_return ResponseData{ .status_code = 200, .body = read_file(file) };
		}
		auto response = co_await upstream_->do_request(std::move(request));
		if (response && response->status_code >= 200 && response->status_code < 300) {
			write_file(file, response->body);
		}
		co_return response;
	}

	static std::string cache_name(std::string_view url, const UrlParameters& params, std::string_view body) {
		std::string key(url);
		for (const auto& [name, value] : params) {
			key += '&';
			key += name;
			key += '=';
			key += value;
		}
		key += body;

		// A human-readable tail of the URL plus a hash of the full key: the tail
		// makes files easy to spot, the hash keeps distinct requests distinct.
		std::string readable;
		for (char c : url) {
			readable += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
		}
		if (readable.size() > 60) {
			readable = readable.substr(readable.size() - 60);
		}
		return readable + '_' + std::to_string(std::hash<std::string>{}(key)) + ".resp";
	}

	static std::string read_file(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	}

	static void write_file(const std::filesystem::path& path, std::string_view body) {
		std::ofstream stream(path, std::ios::binary);
		stream.write(body.data(), static_cast<std::streamsize>(body.size()));
	}

	std::filesystem::path          cache_dir_;
	std::shared_ptr<ClientContext> upstream_;
};

} // namespace aniparse::testing
