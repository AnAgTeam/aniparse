/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "TestRequestContext.hpp"
#include "CoroTest.hpp"

#include <aniparse/runtime/RequestContext.hpp>
#include <aniparse/images/ImagesGetter.hpp>

#include <memory>
#include <optional>

using namespace aniparse;

namespace {

// The default ImagesGetter/ImageContainerGetter methods ignore the context, so this
// client only needs to exist; its request paths are never taken.
struct NullClient : HttpClient {
	RequestTask<HttpResponse> do_request(ConfiguredGetRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "");
	}
	void apply_client_config(HttpClientConfig) override {
	}
	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return nullptr;
	}
};

RequestContext make_context() {
	return test_context(std::make_shared<NullClient>(), nullptr, std::make_shared<ParserConfig>());
}

// Implements only the pure-virtual surface; every optional override keeps its base
// default (NotImplemented / delegating / empty).
struct StubImagesGetter : ImagesGetter {
	RequestTask<std::unique_ptr<ImageContainerGetter>> from_serialized(SerializedGetterData) const override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "");
	}
};

struct StubImageContainerGetter : ImageContainerGetter {
	ImageContainerCapabilities capabilities() const noexcept override {
		return {};
	}
	RequestTask<ImageContainerInfo> load_info(RequestContext) const override {
		ImageContainerInfo out;
		out.common.title = "stub";
		co_return out;
	}
	RequestTask<PageResults<ImageItem>> items(RequestContext, GetFilters) const override {
		co_return PageResults<ImageItem>{};
	}
	RequestTask<SerializedGetterData> serialize() const override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "");
	}
};

} // namespace

TEST_CASE("ImagesGetter default support declarations are empty; suggestions off") {
	StubImagesGetter getter;

	auto support = coro::sync_wait(getter.load_search_capabilities(make_context()));
	REQUIRE(support);
	CHECK(support->supported_filters.empty());
	CHECK(support->supported_sorts.empty());
	CHECK(support->supported_suggestion_kinds.empty());
	CHECK_FALSE(support->capabilities.has(capabilities_flags::supports_suggestions));

	ImagesGetterCapabilities latest = getter.latest_capabilities();
	CHECK(latest.supported_sorts.empty());
}

CORO_TEST_CASE("ImagesGetter search/latest/suggest/parse_url/from_serialized default to NotImplemented") {
	StubImagesGetter getter;
	RequestContext context = make_context();

	auto searched = co_await getter.search(context, SearchRequestQuery{}, GetFilters{});
	REQUIRE_FALSE(searched.has_value());
	CHECK(searched.error().code == RequestErrorCode::NotImplemented);

	auto latest = co_await getter.latest(context, GetFilters{});
	REQUIRE_FALSE(latest.has_value());
	CHECK(latest.error().code == RequestErrorCode::NotImplemented);

	auto suggested = co_await getter.suggest(context, "cir", std::nullopt);
	REQUIRE_FALSE(suggested.has_value());
	CHECK(suggested.error().code == RequestErrorCode::NotImplemented);

	auto parsed = co_await getter.parse_url(context, ParsedUrl::parse("https://example.com/posts/1").value());
	REQUIRE_FALSE(parsed.has_value());
	CHECK(parsed.error().code == RequestErrorCode::NotImplemented);

	auto restored = co_await getter.from_serialized(SerializedGetterData{});
	REQUIRE_FALSE(restored.has_value());
	CHECK(restored.error().code == RequestErrorCode::NotImplemented);
}

CORO_TEST_CASE("ImageContainerGetter comments default to NotImplemented; preview_info knows nothing") {
	StubImageContainerGetter getter;
	RequestContext context = make_context();

	auto comments = co_await getter.load_comments(context, GetFilters{});
	REQUIRE_FALSE(comments.has_value());
	CHECK(comments.error().code == RequestErrorCode::NotImplemented);

	// A getter handed no card says so, and does NOT quietly fetch info() instead.
	CHECK_FALSE(getter.preview_info().has_value());
}
