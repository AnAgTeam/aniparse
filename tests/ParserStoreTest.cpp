/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <aniparse/ParserStore.hpp>

struct TestParser : aniparse::Parser {

	std::string name() const override {
		return "Test parser";
	}

	std::string identifier() const override {
		return "TestParser";
	}

	bool valid_for_url(const aniparse::ParsedUrl& url) const override {
		return !url.host().starts_with("aaa.");
	}

	aniparse::ParserCompatibilities compatibilities() const override {
		return {
			.flags = aniparse::compatibilities_flags::supports_anime_store
		};
	}

	void emplace_domains(aniparse::EmplaceDomainsContext& context) const override {
		context.add_domain("www.youtube.com");
		context.add_domain("youtu.be");
	}
};

// A parser whose identifier is set at construction, so a test can register several
// distinct ones (add_parser rejects a duplicate identifier).
struct NamedParser : aniparse::Parser {
	explicit NamedParser(std::string id) : id_(std::move(id)) {}
	std::string name() const override { return id_; }
	std::string identifier() const override { return id_; }
	aniparse::ParserCompatibilities compatibilities() const override { return {}; }
	void emplace_domains(aniparse::EmplaceDomainsContext&) const override {}
	std::string id_;
};

TEST_CASE("ParserStore parsers() is empty for a fresh store") {
	aniparse::ParserStore parser_store;
	CHECK(parser_store.parsers().empty());
}

TEST_CASE("ParserStore parsers() enumerates every registered parser in identifier order") {
	aniparse::ParserStore parser_store;
	parser_store.add_parser(std::make_unique<NamedParser>("Kitsu"));
	parser_store.add_parser(std::make_unique<NamedParser>("AniList"));
	parser_store.add_parser(std::make_unique<NamedParser>("Danbooru"));

	auto all = parser_store.parsers();
	REQUIRE(all.size() == 3);
	// parsers_ is a std::map keyed by identifier -> enumeration is id-sorted.
	CHECK(all[0]->identifier() == "AniList");
	CHECK(all[1]->identifier() == "Danbooru");
	CHECK(all[2]->identifier() == "Kitsu");
}

TEST_CASE("ParserStore add") {
	aniparse::ParserStore parser_store;

	parser_store.add_parser(std::make_unique<TestParser>());
	// Cannot add another parser with the same identifier()
	REQUIRE_THROWS_AS(parser_store.add_parser(std::make_unique<TestParser>()), std::logic_error);
}

TEST_CASE("ParserStore find") {
	aniparse::ParserStore parser_store;
	parser_store.add_parser(std::make_unique<TestParser>());

	// Cannot find added 1st domain: no "www." subdomain
	CHECK_FALSE(parser_store.find_for_url("https://youtube.com"));
	// Find added 1st domain
	CHECK(parser_store.find_for_url("https://www.youtube.com"));
	// Find added 1st domain with subdomain
	CHECK(parser_store.find_for_url("https://aaa2.www.youtube.com"));
	// Find added 2nd domain
	CHECK(parser_store.find_for_url("https://youtu.be"));
	// Find added 2nd domain with another protocol
	CHECK(parser_store.find_for_url("http://youtu.be"));
	// Find added 2nd domain with subdomain and path
	CHECK(parser_store.find_for_url("http://www.youtu.be/watch?v=123456"));
	// Cannot find: valid_for_url() returns false for aaa. subdomains
	CHECK_FALSE(parser_store.find_for_url("http://aaa.youtube.com"));

	CHECK(parser_store.find_by_key("TestParser"));
	CHECK_FALSE(parser_store.find_by_key("SomeParser"));
}

TEST_CASE("ParserStore refresh_domains adds and drops volatile domains") {
	aniparse::ParserStore parser_store;
	parser_store.add_parser(std::make_unique<TestParser>());

	// A domain the parser never declared statically does not route yet.
	CHECK_FALSE(parser_store.find_for_url("https://example.org"));

	// A catalog refresh adds a volatile domain for the parser: it now routes, and
	// the static domains keep working (volatile is merged on top of static).
	parser_store.refresh_domains({ { "TestParser", { "example.org" } } });
	CHECK(parser_store.find_for_url("https://example.org"));
	CHECK(parser_store.find_for_url("https://www.youtube.com"));
	// valid_for_url still gates the volatile domain like any other.
	CHECK_FALSE(parser_store.find_for_url("https://aaa.example.org"));

	// An empty refresh falls back to static domains only: the volatile domain is
	// dropped, the static ones remain.
	parser_store.refresh_domains({});
	CHECK_FALSE(parser_store.find_for_url("https://example.org"));
	CHECK(parser_store.find_for_url("https://www.youtube.com"));
}