/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include "CoroTest.hpp"
#include <aniparse/video/VideoExtractor.hpp>

namespace {

struct TestExtractor final : aniparse::VideoExtractor {
	TestExtractor(std::string identifier, std::string domain, std::string path_prefix)
	    : identifier_(std::move(identifier)), domain_(std::move(domain)), path_prefix_(std::move(path_prefix)) {}

	std::string identifier() const override { return identifier_; }
	void emplace_domains(aniparse::EmplaceDomainsContext& context) const override {
		context.add_domain(domain_);
	}
	bool valid_for_url(const aniparse::ParsedUrl& url) const override {
		return url.path().starts_with(path_prefix_);
	}
	aniparse::NetworkRequestTask<aniparse::VideoExtraction> extract(
	    aniparse::RequestorContext,
	    aniparse::ParsedUrl) const override {
		co_return outcome;
	}

	std::string identifier_;
	std::string domain_;
	std::string path_prefix_;
	aniparse::VideoExtraction outcome; ///< canned result returned by extract()
};

// The stub extractors never touch the context, so this client only needs to
// exist; its request paths are never taken.
struct NullClient final : aniparse::ClientContext {
	aniparse::NetworkRequestTask<aniparse::ResponseData> do_request(aniparse::ConfiguredGetRequest) override {
		co_return aniparse::make_response_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	aniparse::NetworkRequestTask<aniparse::ResponseData> do_request(aniparse::ConfiguredPostRequest) override {
		co_return aniparse::make_response_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	aniparse::NetworkRequestTask<aniparse::ResponseData> do_request(aniparse::ConfiguredPostMultipartRequest) override {
		co_return aniparse::make_response_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	void set_config(aniparse::ClientConfig) override {}
	std::shared_ptr<aniparse::CookieJar> make_cookie_jar() override { return nullptr; }
};

aniparse::RequestorContext make_context() {
	return aniparse::RequestorContext(std::make_shared<NullClient>(), nullptr,
	                                  std::make_shared<aniparse::ParserConfig>());
}

aniparse::VideoStream test_stream(std::string url, int quality) {
	return aniparse::VideoStream{ .url = std::move(url), .quality = quality, .is_hls = true };
}

} // namespace

TEST_CASE("VideoExtractorStore routes by static domain then exact URL gate") {
	aniparse::VideoExtractorStore store;
	auto edit = store.begin_edit();
	edit.add_extractor(std::make_shared<TestExtractor>("Specific", "video.example", "/watch/"));
	edit.add_extractor(std::make_shared<TestExtractor>("Other", "other.example", "/"));
	edit.commit();

	auto route = store.route_url("https://cdn.video.example/watch/123");
	REQUIRE(route);
	CHECK(route->extractor->identifier() == "Specific");
	CHECK(route->url.path() == "/watch/123");
	CHECK_FALSE(store.route_url("https://video.example/embed/123"));
	CHECK_FALSE(store.route_url("https://unknown.example/watch/123"));
}

TEST_CASE("VideoExtractorStore routes delegated extraction links") {
	aniparse::VideoExtractorStore store;
	auto edit = store.begin_edit();
	edit.add_extractor(std::make_shared<TestExtractor>("First", "first.example", "/"));
	edit.add_extractor(std::make_shared<TestExtractor>("Second", "second.example", "/"));
	edit.commit();

	auto routed = store.route_link({ .url = "https://second.example/video", .extractor_id = "Second" });
	REQUIRE(routed);
	CHECK(routed->extractor->identifier() == "Second");
	CHECK_FALSE(store.route_link({ .url = "https://second.example/video", .extractor_id = "Missing" }));
}

TEST_CASE("VideoExtractorStore edit publishes one complete extractor snapshot") {
	aniparse::VideoExtractorStore store;
	auto edit = store.begin_edit();
	edit.add_extractor(std::make_shared<TestExtractor>("Video", "video.example", "/"));
	CHECK_FALSE(store.route_url("https://video.example/item"));
	edit.commit();
	CHECK(store.route_url("https://video.example/item"));
}

TEST_CASE("VideoExtractorStore rejects null and duplicate extractor registrations") {
	aniparse::VideoExtractorStore store;
	REQUIRE_THROWS_AS(store.add_extractor(nullptr), std::invalid_argument);
	store.add_extractor(std::make_shared<TestExtractor>("Video", "video.example", "/"));
	REQUIRE_THROWS_AS(
	    store.add_extractor(std::make_shared<TestExtractor>("Video", "second.example", "/")),
	    std::logic_error);
}

TEST_CASE("VideoExtractorStore refresh_domains routes catalog-supplied domains") {
	aniparse::VideoExtractorStore store;
	store.add_extractor(std::make_shared<TestExtractor>("Video", "video.example", "/"));

	// Only the static domain routes before any refresh.
	CHECK(store.route_url("https://video.example/item"));
	CHECK_FALSE(store.route_url("https://video-mirror.example/item"));

	store.refresh_domains({ { "Video", { "video-mirror.example" } } });
	// The volatile domain routes on top of the kept static one.
	CHECK(store.route_url("https://video-mirror.example/item"));
	CHECK(store.route_url("https://video.example/item"));

	// An empty override set falls back to static domains only.
	store.refresh_domains({});
	CHECK_FALSE(store.route_url("https://video-mirror.example/item"));
	CHECK(store.route_url("https://video.example/item"));
}

TEST_CASE("VideoExtractor mirrors resolve the catalog override by extractor identifier") {
	using namespace aniparse;
	TestExtractor extractor("Video", "video.example", "/");

	auto services = std::make_shared<ServiceState>();
	services->client = std::make_shared<NullClient>();
	services->mirrors = std::make_shared<MirrorSourceHolder>();
	RequestorContext context(services, std::make_shared<ParserConfig>());

	static constexpr std::string_view builtin[] = { "https://video.example" };
	// No override yet: the built-in list wins.
	CHECK(extractor.base_url(context, builtin) == "https://video.example");

	services->mirrors->set(std::make_shared<const MirrorSource>(
	    std::map<std::string, std::vector<std::string>, std::less<>>{
	        { "Video", { "https://live.example", "https://alt.example" } } }));

	// The mirror snapshot is taken at context construction, so the swapped source
	// is seen through a context built after the swap.
	RequestorContext updated(services, std::make_shared<ParserConfig>());
	auto view = extractor.mirrors(updated, builtin);
	CHECK(view.count() == 2);
	CHECK(extractor.base_url(updated, builtin) == "https://live.example");

	// An extractor without an override keeps its built-in fallback.
	TestExtractor other("Other", "other.example", "/");
	CHECK(other.base_url(updated, builtin) == "https://video.example");
}

CORO_TEST_CASE("VideoExtractorStore extract routes and returns the extractor's streams") {
	using namespace aniparse;
	VideoExtractorStore store;
	auto direct = std::make_shared<TestExtractor>("Direct", "video.example", "/watch/");
	direct->outcome.streams.push_back(test_stream("https://cdn.video.example/1/master.m3u8", 720));
	direct->outcome.headers.set("Referer", "https://video.example/");
	store.add_extractor(std::move(direct));

	auto result = co_await store.extract(make_context(), "https://video.example/watch/1");
	REQUIRE(result);
	REQUIRE(result->streams.size() == 1);
	CHECK(result->streams.front().url == "https://cdn.video.example/1/master.m3u8");
	CHECK(result->streams.front().quality == 720);
	CHECK(result->headers.get("Referer") == "https://video.example/");
}

CORO_TEST_CASE("VideoExtractorStore extract chases delegated links to the first streams") {
	using namespace aniparse;
	VideoExtractorStore store;
	auto first = std::make_shared<TestExtractor>("First", "first.example", "/");
	first->outcome.links.push_back({ .url = "https://second.example/video", .extractor_id = std::nullopt });
	auto second = std::make_shared<TestExtractor>("Second", "second.example", "/");
	second->outcome.streams.push_back(test_stream("https://cdn.second.example/v/720.mp4", 720));
	store.add_extractor(std::move(first));
	store.add_extractor(std::move(second));

	auto result = co_await store.extract(make_context(), "https://first.example/embed");
	REQUIRE(result);
	REQUIRE(result->streams.size() == 1);
	CHECK(result->streams.front().url == "https://cdn.second.example/v/720.mp4");
}

CORO_TEST_CASE("VideoExtractorStore extract caps a delegated-link cycle") {
	using namespace aniparse;
	VideoExtractorStore store;
	auto looping = std::make_shared<TestExtractor>("Loop", "loop.example", "/");
	looping->outcome.links.push_back({ .url = "https://loop.example/next", .extractor_id = std::nullopt });
	store.add_extractor(std::move(looping));

	// The cap cuts the chain and returns the deepest extraction unresolved:
	// no streams, but the not-followed delegated link stays visible.
	auto result = co_await store.extract(make_context(), "https://loop.example/start", 2);
	REQUIRE(result);
	CHECK(result->streams.empty());
	REQUIRE(result->links.size() == 1);
	CHECK(result->links.front().url == "https://loop.example/next");
}

CORO_TEST_CASE("VideoExtractorStore extract rejects unrouted and malformed URLs") {
	using namespace aniparse;
	VideoExtractorStore store;
	store.add_extractor(std::make_shared<TestExtractor>("Video", "video.example", "/"));

	auto unrouted = co_await store.extract(make_context(), "https://unknown.example/watch/1");
	REQUIRE_FALSE(unrouted);
	CHECK(unrouted.error().code == RequestErrorCode::NotImplemented);

	auto malformed = co_await store.extract(make_context(), "not a url");
	REQUIRE_FALSE(malformed);
	CHECK(malformed.error().code == RequestErrorCode::InvalidArguments);
}
