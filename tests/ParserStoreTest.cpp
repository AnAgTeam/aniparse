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

	bool valid_for_url(std::string_view url) const override {
		return url.compare(8, 4, "aaa.") != 0;
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
	REQUIRE(!parser_store.find_for_url("https://youtube.com"));
	// Find added 1st domain
	REQUIRE(parser_store.find_for_url("https://www.youtube.com"));
	// Find added 1st domain with subdomain
	REQUIRE(parser_store.find_for_url("https://aaa2.www.youtube.com"));
	// Find added 2nd domain
	REQUIRE(parser_store.find_for_url("https://youtu.be"));
	// Find added 2nd domain with another protocol
	REQUIRE(parser_store.find_for_url("http://youtu.be"));
	// Find added 2nd domain with subdomain and path
	REQUIRE(parser_store.find_for_url("http://www.youtu.be/watch?v=123456"));
	// Cannot find: valid_for_url() returns false for aaa. subdomains
	REQUIRE(!parser_store.find_for_url("http://aaa.youtube.com"));

	REQUIRE(parser_store.find_by_key("TestParser"));
	REQUIRE(!parser_store.find_by_key("SomeParser"));
}