/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/Catalog.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace aniparse;

namespace {
// Records what it was asked to verify and answers a fixed yes/no, so tests drive
// the crypto seam without a real key.
struct StubVerifier : SignatureVerifier {
	bool answer;
	explicit StubVerifier(bool answer) : answer(answer) {}
	bool verify(std::string_view, std::string_view) const override { return answer; }
};

constexpr std::string_view good_payload = R"({
    "schema_version": 1,
    "revision": 7,
    "parsers": {
        "ExampleParser": { "domains": ["example.com", "example.org"] },
        "OtherParser":   { "domains": ["other.example"] }
    }
})";
} // namespace

TEST_CASE("decode_catalog accepts a signed, well-formed payload") {
	StubVerifier verifier(true);
	auto result = decode_catalog(good_payload, "sig", verifier);
	REQUIRE(result.has_value());
	CHECK(result->revision == 7);
	REQUIRE(result->domains.contains("ExampleParser"));
	CHECK(result->domains.at("ExampleParser") == std::vector<std::string>{ "example.com", "example.org" });
	CHECK(result->domains.at("OtherParser") == std::vector<std::string>{ "other.example" });
}

TEST_CASE("decode_catalog rejects a bad signature before parsing") {
	StubVerifier verifier(false);
	auto result = decode_catalog(good_payload, "sig", verifier);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == CatalogError::BadSignature);
}

TEST_CASE("decode_catalog rejects malformed JSON") {
	StubVerifier verifier(true);
	auto result = decode_catalog("{ not json", "sig", verifier);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == CatalogError::BadFormat);
}

TEST_CASE("decode_catalog rejects an unsupported schema version") {
	StubVerifier verifier(true);
	std::string_view payload = R"({ "schema_version": 2, "revision": 7, "parsers": {} })";
	auto result = decode_catalog(payload, "sig", verifier);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == CatalogError::UnsupportedVersion);
}

TEST_CASE("decode_catalog rejects a non-newer revision (anti-rollback)") {
	StubVerifier verifier(true);
	// min_revision == payload revision -> not strictly newer.
	auto result = decode_catalog(good_payload, "sig", verifier, 7);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == CatalogError::StaleRevision);
}

TEST_CASE("decode_catalog accepts a strictly newer revision") {
	StubVerifier verifier(true);
	auto result = decode_catalog(good_payload, "sig", verifier, 6);
	REQUIRE(result.has_value());
	CHECK(result->revision == 7);
}

TEST_CASE("decode_catalog reads the flat top-level selectors table") {
	StubVerifier verifier(true);
	std::string_view payload = R"({
        "schema_version": 1,
        "revision": 2,
        "parsers": {},
        "selectors": { "example.info.description": "#desc", "example.card.link": "a.card" }
    })";
	auto result = decode_catalog(payload, "sig", verifier);
	REQUIRE(result.has_value());
	REQUIRE(result->selectors.contains("example.info.description"));
	CHECK(result->selectors.at("example.info.description") == "#desc");
	CHECK(result->selectors.at("example.card.link") == "a.card");
}

TEST_CASE("decode_catalog treats a missing required field as bad format") {
	StubVerifier verifier(true);
	std::string_view no_revision = R"({ "schema_version": 1, "parsers": {} })";
	auto result = decode_catalog(no_revision, "sig", verifier);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == CatalogError::BadFormat);
}
