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
		return true;
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
	REQUIRE_THROWS_AS(parser_store.add_parser(std::make_unique<TestParser>()), std::logic_error);

	REQUIRE(!parser_store.find_for_url("https://youtube.com"));
	REQUIRE(parser_store.find_for_url("https://www.youtube.com"));
	REQUIRE(parser_store.find_for_url("https://youtu.be"));
	REQUIRE(parser_store.find_for_url("http://youtu.be"));

	REQUIRE(parser_store.find_by_key("TestParser"));
	REQUIRE(!parser_store.find_by_key("SomeParser"));
}