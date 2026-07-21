/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"

#include <aniparse/ClientContext.hpp>
#include <aniparse/images/Image.hpp>

#include <memory>
#include <optional>

using namespace aniparse;

namespace {

// The default ImagesGetter/ImageContainerGetter methods ignore the context, so this
// client only needs to exist; its request paths are never taken.
struct NullClient : ClientContext {
	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	void set_config(ClientConfig) override {
	}
	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return nullptr;
	}
};

RequestorContext make_context() {
	return RequestorContext(std::make_shared<NullClient>(), nullptr, std::make_shared<ParserConfig>());
}

// Implements only the pure-virtual surface; every optional override keeps its base
// default (NotImplemented / delegating / empty).
struct StubImagesGetter : ImagesGetter {
	NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> from_serialized(SerializedGetterData) const override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
};

struct StubImageContainerGetter : ImageContainerGetter {
	ImageContainerCompatibilities compatibilities() const noexcept override {
		return {};
	}
	NetworkRequestTask<ImageContainerInfo> info(RequestorContext) const override {
		ImageContainerInfo out;
		out.common.title = "stub";
		co_return out;
	}
	NetworkRequestTask<PageResults<ImageItem>> items(RequestorContext, GetFilters) const override {
		co_return PageResults<ImageItem>{};
	}
	NetworkRequestTask<SerializedGetterData> serialize() const override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
};

} // namespace

TEST_CASE("ImagesGetter default support declarations are empty; suggestions off") {
	StubImagesGetter getter;

	auto support = coro::sync_wait(getter.search_support(make_context()));
	REQUIRE(support);
	CHECK(support->supported_filters.empty());
	CHECK(support->supported_sorts.empty());
	CHECK(support->supported_suggestion_kinds.empty());
	CHECK_FALSE(support->compatibilities.has(compatibilities_flags::supports_suggestions));

	ImagesGetterRootCompatibilities latest = getter.latest_support();
	CHECK(latest.supported_sorts.empty());
}

CORO_TEST_CASE("ImagesGetter search/latest/suggest/parse_url/from_serialized default to NotImplemented") {
	StubImagesGetter getter;
	RequestorContext context = make_context();

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
	RequestorContext context = make_context();

	auto comments = co_await getter.comments(context, GetFilters{});
	REQUIRE_FALSE(comments.has_value());
	CHECK(comments.error().code == RequestErrorCode::NotImplemented);

	// A getter handed no card says so, and does NOT quietly fetch info() instead.
	CHECK_FALSE(getter.preview_info().has_value());
}
