/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "TestRequestContext.hpp"
#include "catch_amalgamated.hpp"
#include "CoroTest.hpp"
#include <aniparse/media/ResourceAdapter.hpp>
#include <aniparse/video/VideoExtractor.hpp>

namespace {

struct TestExtractor final : aniparse::VideoExtractor {
	TestExtractor(std::string identifier, std::string domain, std::string path_prefix)
	    : identifier_(std::move(identifier)), domain_(std::move(domain)), path_prefix_(std::move(path_prefix)) {}

	std::string identifier() const override { return identifier_; }
	void emplace_domains(aniparse::EmplaceDomainsSink& sink) const override {
		sink.add_domain(domain_);
	}
	bool valid_for_url(const aniparse::ParsedUrl& url) const override {
		return url.path().starts_with(path_prefix_);
	}
	aniparse::RequestTask<aniparse::VideoExtraction> extract(
	    aniparse::RequestContext,
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
struct NullClient final : aniparse::HttpClient {
	aniparse::RequestTask<aniparse::HttpResponse> do_request(aniparse::ConfiguredGetRequest) override {
		co_return aniparse::make_request_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	aniparse::RequestTask<aniparse::HttpResponse> do_request(aniparse::ConfiguredPostRequest) override {
		co_return aniparse::make_request_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	aniparse::RequestTask<aniparse::HttpResponse> do_request(aniparse::ConfiguredPostMultipartRequest) override {
		co_return aniparse::make_request_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	void apply_client_config(aniparse::HttpClientConfig) override {}
	std::shared_ptr<aniparse::CookieJar> make_cookie_jar() override { return nullptr; }
};

aniparse::RequestContext make_context() {
	return test_context(std::make_shared<NullClient>(), nullptr,
	                                  std::make_shared<aniparse::ParserConfig>());
}

aniparse::VideoStream test_stream(std::string url, int quality) {
	return aniparse::VideoStream{ .url = std::move(url), .quality = quality, .is_hls = true };
}

class NoopAdapter final : public aniparse::ResourceAdapter {
public:
	aniparse::RequestTask<aniparse::OpenedResource> open(
	    aniparse::RequestContext,
	    aniparse::ResourceContext resource) const override {
		co_return aniparse::OpenedResource{
		    .request = {
		        .url = std::move(resource.resource.url),
		        .headers = std::move(resource.resource.headers),
		    },
		};
	}
};

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

	auto services = std::make_shared<RuntimeServices>();
	services->client = std::make_shared<NullClient>();

	auto context = test_context(services, std::make_shared<ParserConfig>());

	static constexpr std::string_view builtin[] = { "https://video.example" };
	// No override yet: the built-in list wins.
	CHECK(extractor.base_url(context, builtin) == "https://video.example");

	services->overrides->store(std::make_shared<const OverridesState>(std::make_shared<const MirrorSource>(
	    std::map<std::string, std::vector<std::string>, std::less<>>{
	        { "Video", { "https://live.example", "https://alt.example" } } })));

	// The mirror snapshot is taken at context construction, so the swapped source
	// is seen through a context built after the swap.
	auto updated = test_context(services, std::make_shared<ParserConfig>());
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
	direct->outcome.playback.streams.push_back(test_stream("https://cdn.video.example/1/master.m3u8", 720));
	direct->outcome.playback.headers.set("Referer", "https://video.example/");
	store.add_extractor(std::move(direct));

	auto result = co_await store.extract(make_context(), "https://video.example/watch/1");
	REQUIRE(result);
	REQUIRE(result->playback.streams.size() == 1);
	CHECK(result->playback.streams.front().url == "https://cdn.video.example/1/master.m3u8");
	CHECK(result->playback.streams.front().quality == 720);
	CHECK(result->playback.headers.get("Referer") == "https://video.example/");
}

CORO_TEST_CASE("VideoExtractorStore extract chases delegated links to the first streams") {
	using namespace aniparse;
	VideoExtractorStore store;
	auto first = std::make_shared<TestExtractor>("First", "first.example", "/");
	first->outcome.links.push_back({ .url = "https://second.example/video", .extractor_id = std::nullopt });
	first->outcome.playback.headers.set("Referer", "https://first.example/");
	first->outcome.playback.adapter = std::make_shared<NoopAdapter>();
	first->outcome.playback.subtitles.push_back({.url = "https://first.example/captions.ass", .format = "ass"});
	auto second = std::make_shared<TestExtractor>("Second", "second.example", "/");
	second->outcome.playback.streams.push_back(test_stream("https://cdn.second.example/v/720.mp4", 720));
	second->outcome.playback.headers.set("Origin", "https://second.example");
	second->outcome.playback.adapter = std::make_shared<NoopAdapter>();
	store.add_extractor(std::move(first));
	store.add_extractor(std::move(second));

	auto result = co_await store.extract(make_context(), "https://first.example/embed");
	REQUIRE(result);
	REQUIRE(result->playback.streams.size() == 1);
	CHECK(result->playback.streams.front().url == "https://cdn.second.example/v/720.mp4");
	CHECK(result->playback.headers.get("Referer") == "https://first.example/");
	CHECK(result->playback.headers.get("Origin") == "https://second.example");
	REQUIRE(result->playback.subtitles.size() == 1);
	CHECK(result->playback.subtitles.front().format == "ass");
	CHECK(result->playback.adapter);
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
	CHECK(result->playback.streams.empty());
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

TEST_CASE("Playback resources preserve source subtitles and prioritize extraction headers") {
    using namespace aniparse;
    VideoPlaybackInfo source;
    source.subtitles = {{.url = "https://media.example/shared.vtt", .format = "vtt"},
                        {.url = "https://media.example/source.ass", .format = "ass"}};
    source.headers.set("Referer", "https://source.example/");
    source.headers.set("Origin", "https://source.example");
    source.adapter = std::make_shared<NoopAdapter>();
    VideoPlaybackInfo extracted;
    extracted.streams.push_back(test_stream("https://media.example/video.m3u8", 720));
    extracted.subtitles = {{.url = "https://media.example/shared.vtt", .format = "vtt", .label = "Resolved"}};
    extracted.headers.set("Referer", "https://player.example/");
    extracted.merge_fallback(std::move(source));
    REQUIRE(extracted.subtitles.size() == 2);
    CHECK(extracted.subtitles[0].label == "Resolved");
    CHECK(extracted.subtitles[1].format == "ass");
    CHECK(extracted.headers.get("Referer") == "https://player.example/");
    CHECK(extracted.headers.get("Origin") == "https://source.example");
    CHECK(extracted.adapter);
    CHECK(extracted.streams.size() == 1);
}
