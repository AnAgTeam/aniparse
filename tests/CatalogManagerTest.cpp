/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/catalog/CatalogManager.hpp>
#include <aniparse/video/VideoExtractor.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace aniparse;

namespace {
struct StubVerifier : SignatureVerifier {
	bool answer;
	explicit StubVerifier(bool answer) : answer(answer) {}
	bool verify(std::string_view, std::string_view) const override { return answer; }
};

// A parser with a chosen identifier and no static domains, so routing to it can
// only come from catalog-supplied volatile domains.
struct CatalogParser : Parser {
	std::string id;
	explicit CatalogParser(std::string id) : id(std::move(id)) {}

	ParserInfo info() const override { return { .name = id }; }
	std::string identifier() const override { return id; }
	bool valid_for_url(const ParsedUrl&) const override { return true; }
	ParserCompatibilities compatibilities() const override { return {}; }
	void emplace_domains(EmplaceDomainsContext&) const override {}
};

struct StaticCatalogParser final : CatalogParser {
	using CatalogParser::CatalogParser;

	void emplace_domains(EmplaceDomainsContext& context) const override {
		context.add_domain("builtin.example");
	}
};

// An extractor with a chosen identifier and no static domains, so routing to it
// can only come from catalog-supplied volatile domains.
struct CatalogExtractor : VideoExtractor {
	std::string id;
	explicit CatalogExtractor(std::string id) : id(std::move(id)) {}

	std::string identifier() const override { return id; }
	void emplace_domains(EmplaceDomainsContext&) const override {}
	bool valid_for_url(const ParsedUrl&) const override { return true; }
	NetworkRequestTask<VideoExtraction> extract(RequestorContext, ParsedUrl) const override {
		co_return VideoExtraction{};
	}
};

std::string catalog(int revision, std::string_view parser_id, std::string_view domain) {
	return "{ \"schema_version\": 1, \"revision\": " + std::to_string(revision) +
	       ", \"parsers\": { \"" + std::string(parser_id) +
	       "\": { \"domains\": [\"" + std::string(domain) + "\"] } } }";
}
} // namespace

TEST_CASE("CatalogManager applies a verified catalog to routing") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("ExampleParser"));
	StubVerifier verifier(true);
	CatalogManager manager(store, verifier);

	// Nothing routes there before any catalog.
	CHECK_FALSE(store.find_for_url("https://example.com"));

	auto applied = manager.apply(catalog(3, "ExampleParser", "example.com"), "sig");
	REQUIRE(applied.has_value());
	CHECK(*applied == 3);
	CHECK(manager.revision() == 3);
	CHECK(store.find_for_url("https://example.com"));
}

TEST_CASE("CatalogManager leaves state unchanged on a bad signature") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("ExampleParser"));
	StubVerifier verifier(false);
	CatalogManager manager(store, verifier);

	auto applied = manager.apply(catalog(3, "ExampleParser", "example.com"), "sig");
	REQUIRE_FALSE(applied.has_value());
	CHECK(applied.error() == CatalogError::BadSignature);
	CHECK(manager.revision() == 0);
	CHECK_FALSE(store.find_for_url("https://example.com"));
}

TEST_CASE("CatalogManager rejects a non-newer catalog and keeps the applied one") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("ExampleParser"));
	StubVerifier verifier(true);
	CatalogManager manager(store, verifier);

	REQUIRE(manager.apply(catalog(5, "ExampleParser", "example.com"), "sig").has_value());
	CHECK(manager.revision() == 5);

	// An older revision is rejected against the tracked revision; the revision-5
	// domains stay in place, the older catalog's domain never lands.
	auto stale = manager.apply(catalog(4, "ExampleParser", "other.example"), "sig");
	REQUIRE_FALSE(stale.has_value());
	CHECK(stale.error() == CatalogError::StaleRevision);
	CHECK(manager.revision() == 5);
	CHECK(store.find_for_url("https://example.com"));
	CHECK_FALSE(store.find_for_url("https://other.example"));
}

TEST_CASE("CatalogManager swaps catalog selectors into the services holder") {
	ParserStore store;
	StubVerifier verifier(true);

	auto services = std::make_shared<ServiceState>();
	services->selectors = std::make_shared<html::SelectorSourceHolder>();
	services->resources = std::make_shared<ResourceCache>();
	CatalogManager manager(store, verifier, services);

	// Before any catalog the holder is empty -> every key falls back.
	CHECK(services->selectors->get()->get("example.info.description", "FALLBACK") == "FALLBACK");

	std::string_view payload = R"({
		"schema_version": 1, "revision": 2, "parsers": {},
		"selectors": { "example.info.description": "#desc" }
	})";
	REQUIRE(manager.apply(payload, "sig").has_value());

	auto source = services->selectors->get();
	REQUIRE(source);
	CHECK(source->get("example.info.description", "FALLBACK") == "#desc"); // override applied
	CHECK(source->get("unknown.key", "FALLBACK") == "FALLBACK");          // unknown still falls back
}

TEST_CASE("CatalogManager swaps catalog patterns into the services holders") {
	ParserStore store;
	StubVerifier verifier(true);

	auto services = std::make_shared<ServiceState>();
	services->patterns = std::make_shared<RegexSourceHolder>();
	services->resources = std::make_shared<ResourceCache>();
	auto extractor_services = std::make_shared<ServiceState>();
	extractor_services->patterns = std::make_shared<RegexSourceHolder>();
	CatalogManager manager(store, verifier, services, nullptr, extractor_services);

	// Before any catalog the holders are empty -> every key falls back.
	CHECK(services->patterns->get()->scoped_to("ExampleParser")->get("info.id", "FALLBACK") ==
	      "FALLBACK");

	std::string_view payload = R"json({
		"schema_version": 1, "revision": 2,
		"parsers": { "ExampleParser": { "patterns": { "info.id": "/x/(?<id>\\d+)" } } },
		"extractors": { "SomeExtractor": { "patterns": { "video_path": "/v/x" } } }
	})json";
	REQUIRE(manager.apply(payload, "sig").has_value());

	// Parser patterns land in the parser holder, extractor patterns in the
	// extractor one — no aliasing across the two.
	CHECK(services->patterns->get()->scoped_to("ExampleParser")->get("info.id", "FALLBACK") ==
	      R"(/x/(?<id>\d+))");
	CHECK(services->patterns->get()->scoped_to("SomeExtractor")->empty_source());
	CHECK(extractor_services->patterns->get()->scoped_to("SomeExtractor")->get("video_path", "FALLBACK") ==
	      "/v/x");
	CHECK(extractor_services->patterns->get()->scoped_to("ExampleParser")->empty_source());
}

TEST_CASE("CatalogManager swaps catalog mirrors and unions their hosts into routing") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("ExampleParser"));
	StubVerifier verifier(true);

	auto services     = std::make_shared<ServiceState>();
	services->mirrors = std::make_shared<MirrorSourceHolder>();
	CatalogManager manager(store, verifier, services);

	// A catalog with only mirrors (no separate "domains") for the parser.
	std::string_view payload = R"({
		"schema_version": 1, "revision": 4,
		"parsers": { "ExampleParser": { "mirrors": ["https://live.example", "https://alt.example:8443/base"] } }
	})";
	REQUIRE(manager.apply(payload, "sig").has_value());

	// The override reached the holder, keyed by parser id.
	auto source = services->mirrors->get();
	REQUIRE(source);
	REQUIRE(source->list_for("ExampleParser"));
	CHECK(*source->list_for("ExampleParser") ==
	      std::vector<std::string>{ "https://live.example", "https://alt.example:8443/base" });

	// Each mirror's host was unioned into routing, even without a "domains" entry —
	// the scheme/port/path are stripped to the bare host.
	CHECK(store.find_for_url("https://live.example/title/1"));
	CHECK(store.find_for_url("https://alt.example/title/1"));
}

TEST_CASE("CatalogManager swaps canonical frontend origins and routes their host") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("ExampleParser"));
	StubVerifier verifier(true);

	auto services     = std::make_shared<ServiceState>();
	services->mirrors = std::make_shared<MirrorSourceHolder>();
	CatalogManager manager(store, verifier, services);

	std::string_view payload = R"({
		"schema_version": 1, "revision": 4,
		"parsers": {
			"ExampleParser": { "canonical_base_url": "https://frontend.example:8443/path" }
		}
	})";
	REQUIRE(manager.apply(payload, "sig").has_value());

	auto source = services->mirrors->get();
	REQUIRE(source);
	REQUIRE(source->canonical_base_for("ExampleParser"));
	CHECK(*source->canonical_base_for("ExampleParser") == "https://frontend.example:8443/path");
	CHECK(store.find_for_url("https://frontend.example/title/1"));
}

TEST_CASE("CatalogManager applies the extractors section to the extractor store and mirrors") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("SharedId"));
	VideoExtractorStore extractors;
	extractors.add_extractor(std::make_shared<CatalogExtractor>("SharedId"));
	StubVerifier verifier(true);

	auto services = std::make_shared<ServiceState>();
	services->mirrors = std::make_shared<MirrorSourceHolder>();
	auto extractor_services = std::make_shared<ServiceState>();
	extractor_services->mirrors = std::make_shared<MirrorSourceHolder>();
	CatalogManager manager(store, verifier, services, &extractors, extractor_services);

	std::string_view payload = R"({
		"schema_version": 1, "revision": 6,
		"parsers": { "SharedId": { "domains": ["parser.example"], "mirrors": ["https://parser-live.example"] } },
		"extractors": { "SharedId": { "mirrors": ["https://extractor-live.example:8443/base"] } }
	})";
	REQUIRE(manager.apply(payload, "sig").has_value());

	// Each side routes only its own hosts, even under the shared identifier.
	CHECK(store.find_for_url("https://parser.example/title/1"));
	CHECK_FALSE(store.find_for_url("https://extractor-live.example/title/1"));
	CHECK_FALSE(extractors.route_url("https://parser.example/watch/1"));
	// The extractor's mirror host was unioned into its routing.
	CHECK(extractors.route_url("https://extractor-live.example/watch/1"));

	// Mirrors went to separate holders: the parser override in services, the
	// extractor override in extractor_services — no aliasing under the shared id.
	REQUIRE(services->mirrors->get()->list_for("SharedId"));
	CHECK(*services->mirrors->get()->list_for("SharedId") ==
	      std::vector<std::string>{ "https://parser-live.example" });
	REQUIRE(extractor_services->mirrors->get()->list_for("SharedId"));
	CHECK(*extractor_services->mirrors->get()->list_for("SharedId") ==
	      std::vector<std::string>{ "https://extractor-live.example:8443/base" });
}

TEST_CASE("CatalogManager reset restores static routing and empty override snapshots") {
	ParserStore store;
	store.add_parser(std::make_unique<StaticCatalogParser>("SharedId"));
	VideoExtractorStore extractors;
	extractors.add_extractor(std::make_shared<CatalogExtractor>("SharedId"));
	StubVerifier verifier(true);

	auto services = std::make_shared<ServiceState>();
	services->mirrors = std::make_shared<MirrorSourceHolder>();
	services->selectors = std::make_shared<html::SelectorSourceHolder>();
	services->patterns = std::make_shared<RegexSourceHolder>();
	services->resources = std::make_shared<ResourceCache>();
	auto extractor_services = std::make_shared<ServiceState>();
	extractor_services->mirrors = std::make_shared<MirrorSourceHolder>();
	extractor_services->patterns = std::make_shared<RegexSourceHolder>();
	CatalogManager manager(store, verifier, services, &extractors, extractor_services);

	std::string_view payload = R"({
		"schema_version": 1, "revision": 6,
		"parsers": {
			"SharedId": {
				"domains": ["catalog.example"],
				"mirrors": ["https://mirror.example"],
				"canonical_base_url": "https://frontend.example",
				"patterns": { "info.id": "/x" }
			}
		},
		"extractors": { "SharedId": { "mirrors": ["https://extractor.example"], "patterns": { "video_path": "/v/x" } } },
		"selectors": { "shared.info": ".catalog" }
	})";
	REQUIRE(manager.apply(payload, "sig").has_value());
	CHECK(store.find_for_url("https://catalog.example/title/1"));
	CHECK(extractors.route_url("https://extractor.example/embed/1"));
	CHECK(services->selectors->get()->get("shared.info", "FALLBACK") == ".catalog");
	CHECK(services->patterns->get()->scoped_to("SharedId")->get("info.id", "FALLBACK") == "/x");
	CHECK(extractor_services->patterns->get()->scoped_to("SharedId")->get("video_path", "FALLBACK") == "/v/x");

	manager.reset();

	CHECK(manager.revision() == 0);
	CHECK(store.find_for_url("https://builtin.example/title/1"));
	CHECK_FALSE(store.find_for_url("https://catalog.example/title/1"));
	CHECK_FALSE(store.find_for_url("https://mirror.example/title/1"));
	CHECK_FALSE(store.find_for_url("https://frontend.example/title/1"));
	CHECK_FALSE(extractors.route_url("https://extractor.example/embed/1"));
	CHECK(services->mirrors->get()->empty_source());
	CHECK(extractor_services->mirrors->get()->empty_source());
	CHECK(services->selectors->get()->get("shared.info", "FALLBACK") == "FALLBACK");
	CHECK(services->patterns->get()->empty_source());
	CHECK(extractor_services->patterns->get()->empty_source());
}

TEST_CASE("CatalogManager without an extractor store ignores the extractors section") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("ExampleParser"));
	StubVerifier verifier(true);
	CatalogManager manager(store, verifier);

	std::string_view payload = R"({
		"schema_version": 1, "revision": 2,
		"parsers": { "ExampleParser": { "domains": ["example.com"] } },
		"extractors": { "SomeExtractor": { "domains": ["video.example"] } }
	})";
	REQUIRE(manager.apply(payload, "sig").has_value());
	CHECK(store.find_for_url("https://example.com"));
}
