/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "catch_amalgamated.hpp"
#include <aniparse/source/Parser.hpp>
#include <aniparse/catalog/CatalogRootGetter.hpp>

using namespace aniparse;

namespace {
class TestProfile final : public ParserProfile {
public:
	using ParserProfile::ParserProfile;
	void configure(ParserConfig& config) const override {
		config.headers.set("X-Profile", info().id);
	}
};

class ProfileParser : public Parser {
public:
	ParserInfo info() const override { return {.name = "Profiles"}; }
	std::string identifier() const override { return "profiles"; }
	void emplace_domains(EmplaceDomainsContext&) const override {}
	std::vector<ProfileInfo> descriptors{{"shared", "Account"}, {"separate", "Other account"}};
	std::vector<ProfileInfo> profile_descriptors() const override { return descriptors; }
	std::vector<CatalogInfo> scope_descriptors() const override {
		return {
			{.id = "manga", .kinds = catalog_kinds::manga, .profile_id = "shared"},
			{.id = "anime", .kinds = catalog_kinds::anime, .profile_id = "shared"},
			{.id = "other", .kinds = catalog_kinds::manga, .profile_id = "separate"},
		};
	}
protected:
	std::unique_ptr<ParserProfile> make_profile(ProfileInfo info) const override {
		return std::make_unique<TestProfile>(identifier(), std::move(info));
	}
	std::unique_ptr<CatalogRootGetter> make_catalog_getter(CatalogInfo scope) const override {
		return std::make_unique<CatalogRootGetter>(std::move(scope));
	}
};
}

TEST_CASE("Catalogs select shared or independent profiles explicitly", "[profiles]") {
	ProfileParser parser;
	auto manga = parser.catalog_getter("manga");
	auto anime = parser.catalog_getter("anime");
	auto other = parser.catalog_getter("other");
	REQUIRE(manga->scope().profile_id == anime->scope().profile_id);
	REQUIRE(other->scope().profile_id != manga->scope().profile_id);
	auto shared = parser.profile(manga->scope().profile_id);
	auto separate = parser.profile(other->scope().profile_id);
	auto first = shared->make_config();
	auto second = shared->make_config();
	auto third = separate->make_config();
	CHECK(first != second);
	CHECK(first->parser_id == "profiles");
	CHECK(first->profile_id == "shared");
	CHECK(third->profile_id == "separate");
	CHECK(first->headers.at("X-Profile") == "shared");
	CHECK(third->headers.at("X-Profile") == "separate");
	first->headers.set("Authorization", "Bearer first-account");
	CHECK_FALSE(second->headers.contains("Authorization"));
	CHECK_FALSE(third->headers.contains("Authorization"));
	CHECK(parser.profile("missing") == nullptr);
}

TEST_CASE("Profile descriptors and scope references are validated", "[profiles]") {
	ProfileParser parser;
	SECTION("empty list") {
		parser.descriptors.clear();
		CHECK_THROWS_AS(parser.profiles(), std::logic_error);
	}
	SECTION("empty ID") {
		parser.descriptors = {{"", "Invalid"}};
		CHECK_THROWS_AS(parser.profiles(), std::logic_error);
	}
	SECTION("duplicate IDs") {
		parser.descriptors.push_back({"shared", "Duplicate"});
		CHECK_THROWS_AS(parser.profiles(), std::logic_error);
	}
	SECTION("missing scope profile") {
		parser.descriptors = {{"separate", "Other"}};
		CHECK_THROWS_AS(parser.scopes(), std::logic_error);
	}
	SECTION("mismatched factory") {
		class WrongParser final : public ProfileParser {
			std::unique_ptr<ParserProfile> make_profile(ProfileInfo info) const override {
				return std::make_unique<TestProfile>("another-parser", std::move(info));
			}
		} wrong;
		CHECK_THROWS_AS(wrong.profile("shared"), std::logic_error);
	}
	CHECK_THROWS_AS(ParserProfile("", {"main", "Default"}), std::invalid_argument);
	CHECK_THROWS_AS(ParserProfile("parser", {"", "Default"}), std::invalid_argument);
}

TEST_CASE("Factory validation distinguishes unknown IDs from broken declared factories", "[profiles]") {
    class BrokenParser final : public ProfileParser {
    public:
        mutable int profile_creations = 0;
        bool null_profile = false;
        bool wrong_profile_id = false;
        bool null_catalog = false;
        bool wrong_catalog_id = false;
        bool wrong_catalog_profile = false;
    protected:
        std::unique_ptr<ParserProfile> make_profile(ProfileInfo info) const override {
            ++profile_creations;
            if (null_profile) return nullptr;
            if (wrong_profile_id) info.id = "wrong";
            return ProfileParser::make_profile(std::move(info));
        }
        std::unique_ptr<CatalogRootGetter> make_catalog_getter(CatalogInfo scope) const override {
            if (null_catalog) return nullptr;
            if (wrong_catalog_id) scope.id = "wrong";
            if (wrong_catalog_profile) scope.profile_id = "separate";
            return ProfileParser::make_catalog_getter(std::move(scope));
        }
    } parser;
    CHECK(parser.profile("missing") == nullptr);
    CHECK(parser.catalog_getter("missing") == nullptr);
    REQUIRE(parser.catalog_getter("manga"));
    CHECK(parser.profile_creations == 0);
    SECTION("null profile") {
        parser.null_profile = true;
        CHECK_THROWS_AS(parser.profile("shared"), std::logic_error);
    }
    SECTION("wrong profile identity") {
        parser.wrong_profile_id = true;
        CHECK_THROWS_AS(parser.profile("shared"), std::logic_error);
    }
    SECTION("null catalog") {
        parser.null_catalog = true;
        CHECK_THROWS_AS(parser.catalog_getter("manga"), std::logic_error);
    }
    SECTION("wrong catalog identity") {
        parser.wrong_catalog_id = true;
        CHECK_THROWS_AS(parser.catalog_getter("manga"), std::logic_error);
    }
    SECTION("wrong catalog profile") {
        parser.wrong_catalog_profile = true;
        CHECK_THROWS_AS(parser.catalog_getter("manga"), std::logic_error);
    }
}

TEST_CASE("Scopes permit sources without catalogs and reject malformed descriptors early", "[profiles]") {
    class DescriptorParser final : public ProfileParser {
    public:
        std::vector<CatalogInfo> catalogs;
    protected:
        std::vector<CatalogInfo> scope_descriptors() const override { return catalogs; }
    } parser;
    CHECK(parser.scopes().empty());
    SECTION("empty scope ID") {
        parser.catalogs = {{.id = "", .profile_id = "shared"}};
        CHECK_THROWS_AS(parser.scopes(), std::logic_error);
    }
    SECTION("duplicate scope IDs") {
        parser.catalogs = {{.id = "same", .profile_id = "shared"}, {.id = "same", .profile_id = "separate"}};
        CHECK_THROWS_AS(parser.scopes(), std::logic_error);
    }
    SECTION("empty profile ID") {
        parser.catalogs = {{.id = "catalog", .profile_id = ""}};
        CHECK_THROWS_AS(parser.scopes(), std::logic_error);
    }
    SECTION("missing profile") {
        parser.catalogs = {{.id = "catalog", .profile_id = "missing"}};
        CHECK_THROWS_AS(parser.scopes(), std::logic_error);
    }
}
