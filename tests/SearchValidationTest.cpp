/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <aniparse/types/Search.hpp>
#include <aniparse/manga/Manga.hpp>

#include <algorithm>

using namespace aniparse;

namespace {

SearchItems make_supported() {
	return {
		{ std::string(search_keys::tag), TextQuery{ .exclusive = true } },
		{ std::string(search_keys::title), TextQuery{ .exclusive = false } },
		{ std::string(search_keys::pages), IntInterval{ .from = 1, .to = 1000, .exclusive = false } },
		{ "language", ItemSelection{
			{ "en", { .exclusive = true } },
			{ "ru", { .exclusive = false } },
		} },
		{ "uncensored", Checkmark{ .exclusive = false } },
	};
}

bool has_error(const std::vector<SearchQueryError>& errors, SearchQueryError::Reason reason, std::string_view key) {
	return std::find(errors.begin(), errors.end(),
		SearchQueryError{ .reason = reason, .key = std::string(key) }) != errors.end();
}

} // namespace

TEST_CASE("Search validation accepts a query matching the declaration") {
	SearchRequestQuery query{
		.query = "school",
		.filters = {
			{ std::string(search_keys::tag), TextQuery{ .text = "vanilla", .exclusive = true } },
			{ std::string(search_keys::pages), IntInterval{ .from = 10, .to = 100 } },
			{ "language", ItemSelection{ { "en", { .enabled = true, .exclusive = true } } } },
			{ "uncensored", Checkmark{} },
		},
	};

	REQUIRE(validate_search_query(make_supported(), query).empty());
}

TEST_CASE("Search validation accepts an empty filter set") {
	REQUIRE(validate_search_query({}, SearchRequestQuery{ .query = "anything" }).empty());
}

TEST_CASE("Search validation rejects an undeclared key") {
	SearchRequestQuery query{
		.filters = { { "made_up", TextQuery{ .text = "x" } } },
	};

	auto errors = validate_search_query(make_supported(), query);
	REQUIRE(errors.size() == 1);
	REQUIRE(has_error(errors, SearchQueryError::Reason::UnknownKey, "made_up"));
}

TEST_CASE("Search validation rejects a wrong value type") {
	SearchRequestQuery query{
		.filters = { { std::string(search_keys::tag), IntInterval{ .from = 1 } } },
	};

	auto errors = validate_search_query(make_supported(), query);
	REQUIRE(errors.size() == 1);
	REQUIRE(has_error(errors, SearchQueryError::Reason::TypeMismatch, search_keys::tag));
}

TEST_CASE("Search validation rejects unsupported exclusion") {
	SearchRequestQuery query{
		.filters = {
			// title declares exclusive = false, pages too
			{ std::string(search_keys::title), TextQuery{ .text = "name", .exclusive = true } },
			{ std::string(search_keys::pages), IntInterval{ .from = 1, .exclusive = true } },
			// the "ru" item forbids exclusion, "en" allows it
			{ "language", ItemSelection{
				{ "en", { .enabled = true, .exclusive = true } },
				{ "ru", { .enabled = true, .exclusive = true } },
			} },
		},
	};

	auto errors = validate_search_query(make_supported(), query);
	REQUIRE(errors.size() == 3);
	REQUIRE(has_error(errors, SearchQueryError::Reason::ExclusionNotSupported, search_keys::title));
	REQUIRE(has_error(errors, SearchQueryError::Reason::ExclusionNotSupported, search_keys::pages));
	REQUIRE(has_error(errors, SearchQueryError::Reason::ExclusionNotSupported, "language"));
}

TEST_CASE("Search validation rejects malformed values") {
	SearchRequestQuery query{
		.filters = {
			{ std::string(search_keys::tag), TextQuery{ .text = "", .exclusive = true } },
			// inverted interval, and 2000 is above the declared max of 1000
			{ std::string(search_keys::pages), IntInterval{ .from = 2000, .to = 5 } },
			{ "language", ItemSelection{ { "jp", { .enabled = true } } } },
		},
	};

	auto errors = validate_search_query(make_supported(), query);
	REQUIRE(errors.size() == 3);
	REQUIRE(has_error(errors, SearchQueryError::Reason::InvalidValue, search_keys::tag));
	REQUIRE(has_error(errors, SearchQueryError::Reason::InvalidValue, search_keys::pages));
	REQUIRE(has_error(errors, SearchQueryError::Reason::InvalidValue, "language"));
}

TEST_CASE("Search validation rejects interval below the declared minimum") {
	SearchRequestQuery query{
		.filters = { { std::string(search_keys::pages), IntInterval{ .from = 0 } } },
	};

	auto errors = validate_search_query(make_supported(), query);
	REQUIRE(errors.size() == 1);
	REQUIRE(has_error(errors, SearchQueryError::Reason::InvalidValue, search_keys::pages));
}

TEST_CASE("Search validation describes all violations in one line") {
	std::vector<SearchQueryError> errors = {
		{ .reason = SearchQueryError::Reason::UnknownKey, .key = "made_up" },
		{ .reason = SearchQueryError::Reason::TypeMismatch, .key = "tag" },
	};

	REQUIRE(describe_search_query_errors(errors)
		== "unknown filter 'made_up'; wrong value type for filter 'tag'");
}

namespace {

struct FilteringRootGetter : MangaRootGetter {
	SearchCompatibilities search_support() const noexcept override {
		return { .supported_filters = make_supported() };
	}

	NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(SerializedGetterData) noexcept override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
};

} // namespace

TEST_CASE("MangaRootGetter validates against its own declaration") {
	FilteringRootGetter getter;

	SearchRequestQuery valid{
		.filters = { { std::string(search_keys::tag), TextQuery{ .text = "vanilla" } } },
	};
	REQUIRE(getter.validate_query(valid).empty());

	SearchRequestQuery invalid{
		.filters = { { "made_up", Checkmark{} } },
	};
	auto errors = getter.validate_query(invalid);
	REQUIRE(errors.size() == 1);
	REQUIRE(has_error(errors, SearchQueryError::Reason::UnknownKey, "made_up"));
}
