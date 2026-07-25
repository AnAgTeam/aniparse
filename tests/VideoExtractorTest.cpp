/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
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
		co_return aniparse::VideoExtraction{};
	}

	std::string identifier_;
	std::string domain_;
	std::string path_prefix_;
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
