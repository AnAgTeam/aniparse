/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/catalog/CatalogManager.hpp>

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
