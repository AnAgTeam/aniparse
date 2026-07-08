/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/CatalogManager.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

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

	std::string name() const override { return id; }
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
	store.add_parser(std::make_unique<CatalogParser>("MangaLib"));
	StubVerifier verifier(true);
	CatalogManager manager(store, verifier);

	// Nothing routes there before any catalog.
	CHECK_FALSE(store.find_for_url("https://mangalib.me"));

	auto applied = manager.apply(catalog(3, "MangaLib", "mangalib.me"), "sig");
	REQUIRE(applied.has_value());
	CHECK(*applied == 3);
	CHECK(manager.revision() == 3);
	CHECK(store.find_for_url("https://mangalib.me"));
}

TEST_CASE("CatalogManager leaves state unchanged on a bad signature") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("MangaLib"));
	StubVerifier verifier(false);
	CatalogManager manager(store, verifier);

	auto applied = manager.apply(catalog(3, "MangaLib", "mangalib.me"), "sig");
	REQUIRE_FALSE(applied.has_value());
	CHECK(applied.error() == CatalogError::BadSignature);
	CHECK(manager.revision() == 0);
	CHECK_FALSE(store.find_for_url("https://mangalib.me"));
}

TEST_CASE("CatalogManager rejects a non-newer catalog and keeps the applied one") {
	ParserStore store;
	store.add_parser(std::make_unique<CatalogParser>("MangaLib"));
	StubVerifier verifier(true);
	CatalogManager manager(store, verifier);

	REQUIRE(manager.apply(catalog(5, "MangaLib", "mangalib.me"), "sig").has_value());
	CHECK(manager.revision() == 5);

	// An older revision is rejected against the tracked revision; the revision-5
	// domains stay in place, the older catalog's domain never lands.
	auto stale = manager.apply(catalog(4, "MangaLib", "other.example"), "sig");
	REQUIRE_FALSE(stale.has_value());
	CHECK(stale.error() == CatalogError::StaleRevision);
	CHECK(manager.revision() == 5);
	CHECK(store.find_for_url("https://mangalib.me"));
	CHECK_FALSE(store.find_for_url("https://other.example"));
}
