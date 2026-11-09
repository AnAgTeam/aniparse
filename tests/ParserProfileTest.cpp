#include "TestRequestContext.hpp"
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
	void emplace_domains(EmplaceDomainsSink&) const override {}
	std::vector<ProfileInfo> descriptors{{"shared", "Account"}, {"separate", "Other account"}};
	std::vector<ProfileInfo> profile_descriptors() const override { return descriptors; }
	std::vector<CatalogInfo> catalog_descriptors() const override {
		return {
			{.id = "manga", .kinds = catalog_kinds::manga, .profile_id = "shared"},
			{.id = "anime", .kinds = catalog_kinds::anime, .profile_id = "shared"},
			{.id = "other", .kinds = catalog_kinds::manga, .profile_id = "separate"},
		};
	}
protected:
	std::unique_ptr<ParserProfile> make_profile_impl(ProfileInfo info) const override {
		return std::make_unique<TestProfile>(std::move(info));
	}
	std::unique_ptr<CatalogRootGetter> make_catalog_getter_impl(CatalogInfo catalog) const override {
		return std::make_unique<CatalogRootGetter>(std::move(catalog));
	}
};
}

TEST_CASE("Catalogs select shared or independent profiles explicitly", "[profiles]") {
	ProfileParser parser;
	auto manga = parser.make_catalog_getter("manga");
	auto anime = parser.make_catalog_getter("anime");
	auto other = parser.make_catalog_getter("other");
	REQUIRE(manga->info().profile_id == anime->info().profile_id);
	REQUIRE(other->info().profile_id != manga->info().profile_id);
	auto shared = parser.make_profile(manga->info().profile_id);
	auto separate = parser.make_profile(other->info().profile_id);
	auto first = shared->make_session(test_services()).config();
	auto second = shared->make_session(test_services()).config();
	auto third = separate->make_session(test_services()).config();
	CHECK(first != second);
	CHECK(first->headers.at("X-Profile") == "shared");
	CHECK(third->headers.at("X-Profile") == "separate");
	first->cookie_jar->set_cookie({.name = "session", .value = "first-account", .domain = "example.com"});
	CHECK(second->cookie_jar->cookies().empty());
	CHECK(third->cookie_jar->cookies().empty());
	CHECK_FALSE(second->headers.contains("Authorization"));
	CHECK_FALSE(third->headers.contains("Authorization"));
	CHECK(parser.make_profile("missing") == nullptr);
}

TEST_CASE("Profile descriptors and catalog references are validated", "[profiles]") {
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
	SECTION("missing catalog profile") {
		parser.descriptors = {{"separate", "Other"}};
		CHECK_THROWS_AS(parser.catalogs(), std::logic_error);
	}
	SECTION("mismatched factory") {
		class WrongParser final : public ProfileParser {
			std::unique_ptr<ParserProfile> make_profile_impl(ProfileInfo info) const override {
				info.id = "another-profile";
				return std::make_unique<TestProfile>(std::move(info));
			}
		} wrong;
		CHECK_THROWS_AS(wrong.make_profile("shared"), std::logic_error);
	}
	CHECK_THROWS_AS(ParserProfile(ProfileInfo{"", "Default"}), std::invalid_argument);
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
        std::unique_ptr<ParserProfile> make_profile_impl(ProfileInfo info) const override {
            ++profile_creations;
            if (null_profile) return nullptr;
            if (wrong_profile_id) info.id = "wrong";
            return ProfileParser::make_profile_impl(std::move(info));
        }
        std::unique_ptr<CatalogRootGetter> make_catalog_getter_impl(CatalogInfo catalog) const override {
            if (null_catalog) return nullptr;
            if (wrong_catalog_id) catalog.id = "wrong";
            if (wrong_catalog_profile) catalog.profile_id = "separate";
            return ProfileParser::make_catalog_getter_impl(std::move(catalog));
        }
    } parser;
    CHECK(parser.make_profile("missing") == nullptr);
    CHECK(parser.make_catalog_getter("missing") == nullptr);
    REQUIRE(parser.make_catalog_getter("manga"));
    CHECK(parser.profile_creations == 0);
    SECTION("null profile") {
        parser.null_profile = true;
        CHECK_THROWS_AS(parser.make_profile("shared"), std::logic_error);
    }
    SECTION("wrong profile identity") {
        parser.wrong_profile_id = true;
        CHECK_THROWS_AS(parser.make_profile("shared"), std::logic_error);
    }
    SECTION("null catalog") {
        parser.null_catalog = true;
        CHECK_THROWS_AS(parser.make_catalog_getter("manga"), std::logic_error);
    }
    SECTION("wrong catalog identity") {
        parser.wrong_catalog_id = true;
        CHECK_THROWS_AS(parser.make_catalog_getter("manga"), std::logic_error);
    }
    SECTION("wrong catalog profile") {
        parser.wrong_catalog_profile = true;
        CHECK_THROWS_AS(parser.make_catalog_getter("manga"), std::logic_error);
    }
}

TEST_CASE("Catalogs permit sources without catalogs and reject malformed descriptors early", "[profiles]") {
    class DescriptorParser final : public ProfileParser {
    public:
        std::vector<CatalogInfo> declared_catalogs;
    protected:
        std::vector<CatalogInfo> catalog_descriptors() const override { return declared_catalogs; }
    } parser;
    CHECK(parser.catalogs().empty());
    SECTION("empty catalog ID") {
        parser.declared_catalogs = {{.id = "", .profile_id = "shared"}};
        CHECK_THROWS_AS(parser.catalogs(), std::logic_error);
    }
    SECTION("duplicate catalog IDs") {
        parser.declared_catalogs = {{.id = "same", .profile_id = "shared"}, {.id = "same", .profile_id = "separate"}};
        CHECK_THROWS_AS(parser.catalogs(), std::logic_error);
    }
    SECTION("empty profile ID") {
        parser.declared_catalogs = {{.id = "catalog", .profile_id = ""}};
        CHECK_THROWS_AS(parser.catalogs(), std::logic_error);
    }
    SECTION("missing profile") {
        parser.declared_catalogs = {{.id = "catalog", .profile_id = "missing"}};
        CHECK_THROWS_AS(parser.catalogs(), std::logic_error);
    }
}
