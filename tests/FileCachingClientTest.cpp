/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include <aniparse/testing/FileCachingClient.hpp>

#include <filesystem>

using namespace aniparse;

namespace {

// Counts how often the upstream is actually hit, so the test can assert a cache
// hit did NOT forward. Deliberately not the curl client: FileCachingClient takes
// any injected ClientContext, so this exercises it with no HTTP backend at all.
struct CountingUpstream : ClientContext {
	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest) override {
		++calls;
		co_return ResponseData{ .status_code = 200, .body = body };
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest) override {
		++calls;
		co_return ResponseData{ .status_code = 200, .body = body };
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest) override {
		++calls;
		co_return ResponseData{ .status_code = 200, .body = body };
	}
	void set_config(ClientConfig) override {}
	std::shared_ptr<CookieJar> make_cookie_jar() override { return nullptr; }

	int         calls = 0;
	std::string body  = "cached-payload";
};

// A cache directory unique to this test, wiped on entry and scope exit.
struct TempCacheDir {
	std::filesystem::path path = std::filesystem::temp_directory_path() / "aniparse_fcc_test";
	TempCacheDir() { std::filesystem::remove_all(path); }
	~TempCacheDir() { std::filesystem::remove_all(path); }
};

ConfiguredGetRequest sample_request() {
	return ConfiguredGetRequest{ .request = GetRequest{ .url = "http://example.test/resource" } };
}

} // namespace

CORO_TEST_CASE("FileCachingClient serves a miss from upstream, then a hit from disk") {
	TempCacheDir cache;
	auto upstream = std::make_shared<CountingUpstream>();
	testing::FileCachingClient client(cache.path, upstream);

	// First call: cache miss -> forwarded upstream, 2xx body written to disk.
	auto first = co_await client.do_request(sample_request());
	REQUIRE(first.has_value());
	REQUIRE(first->body == "cached-payload");
	REQUIRE(upstream->calls == 1);

	// Second identical call: cache hit -> served from disk, upstream untouched.
	auto second = co_await client.do_request(sample_request());
	REQUIRE(second.has_value());
	REQUIRE(second->status_code == 200);
	REQUIRE(second->body == "cached-payload");
	REQUIRE(upstream->calls == 1);
}
