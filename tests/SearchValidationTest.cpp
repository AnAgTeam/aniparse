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
			{ "language", ItemSelection{ { "en", { .exclusive = true } } } },
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
				{ "en", { .exclusive = true } },
				{ "ru", { .exclusive = true } },
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
			{ "language", ItemSelection{ { "jp", {} } } },
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

TEST_CASE("Sort validation accepts declared sorts and the default order") {
	SupportedSorts supported = {
		{ std::string(sort_keys::popularity), { .ascending = true } },
		{ std::string(sort_keys::downloads), {} },
	};

	// No sort requested = the source's default order.
	REQUIRE(validate_sort(supported, std::nullopt).empty());
	REQUIRE(validate_sort(supported, SortOrder{ .key = "popularity", .ascending = true }).empty());
	REQUIRE(validate_sort(supported, SortOrder{ .key = "popularity" }).empty());
	REQUIRE(validate_sort(supported, SortOrder{ .key = "downloads" }).empty());
}

TEST_CASE("Sort validation rejects unknown keys and denied directions") {
	SupportedSorts supported = { { std::string(sort_keys::downloads), {} } };

	auto unknown = validate_sort(supported, SortOrder{ .key = "comments" });
	REQUIRE(unknown.size() == 1);
	REQUIRE(unknown.front().reason == SearchQueryError::Reason::UnknownSortKey);
	REQUIRE(unknown.front().key == "comments");

	// downloads declares descending only
	auto direction = validate_sort(supported, SortOrder{ .key = "downloads", .ascending = true });
	REQUIRE(direction.size() == 1);
	REQUIRE(direction.front().reason == SearchQueryError::Reason::SortDirectionNotSupported);
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

SearchCompatibilities filtering_support() {
	return {
		.supported_filters = make_supported(),
		.supported_sorts   = { { std::string(sort_keys::popularity), {} } },
	};
}

struct FilteringRootGetter : MangaRootGetter {
	NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext) override {
		co_return filtering_support();
	}

	MangaGetterRootCompatibilities latest_support() const noexcept override {
		return { .supported_sorts = { { std::string(sort_keys::update_time), { .ascending = true } } } };
	}

	NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(SerializedGetterData) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
};

} // namespace

TEST_CASE("MangaRootGetter validates against its own declaration") {
	FilteringRootGetter getter;

	SearchRequestQuery valid{
		.filters = { { std::string(search_keys::tag), TextQuery{ .text = "vanilla" } } },
	};
	GetFilters valid_filters{ .sort = SortOrder{ .key = std::string(sort_keys::popularity) } };
	REQUIRE(validate_query(filtering_support(), valid, valid_filters).empty());

	// Filter and sort violations are collected together.
	SearchRequestQuery invalid{
		.filters = { { "made_up", Checkmark{} } },
	};
	GetFilters invalid_filters{ .sort = SortOrder{ .key = "comments" } };
	auto errors = validate_query(filtering_support(), invalid, invalid_filters);
	REQUIRE(errors.size() == 2);
	REQUIRE(has_error(errors, SearchQueryError::Reason::UnknownKey, "made_up"));
	REQUIRE(has_error(errors, SearchQueryError::Reason::UnknownSortKey, "comments"));

	// latest() validates against its own, separate declaration.
	GetFilters latest_filters{ .sort = SortOrder{ .key = std::string(sort_keys::update_time), .ascending = true } };
	REQUIRE(getter.validate_latest_filters(latest_filters).empty());
	REQUIRE_FALSE(getter.validate_latest_filters(invalid_filters).empty());
}

TEST_CASE("Identity keys are told apart from filters") {
	SearchCompatibilities support{ .supported_filters = make_supported() };
	support.supported_filters.emplace(std::string(search_keys::mal_id), ItemSelection{});

	REQUIRE(is_identity_key(support, search_keys::mal_id));
	REQUIRE_FALSE(is_identity_key(support, search_keys::tag));
	// A key the source does not offer is still not a filter to present.
	REQUIRE(is_identity_key(support, search_keys::kitsu_id));
	REQUIRE_FALSE(is_identity_key(support, "made_up"));
}

TEST_CASE("An id namespace maps to the key that looks it up") {
	REQUIRE(search_key_for(id_namespaces::mal) == search_keys::mal_id);
	REQUIRE(search_key_for(id_namespaces::shikimori) == search_keys::shikimori_id);
	// Partial by design: a vocabulary no source looks up by has no key, and the
	// namespace and key spellings are deliberately not the same string.
	REQUIRE(search_key_for(id_namespaces::anidb).empty());
	REQUIRE(search_key_for("made_up").empty());
	REQUIRE(search_key_for(search_keys::mal_id).empty());

	// Every key a namespace maps to is one a consumer can recognise as a lookup.
	SearchCompatibilities support;
	REQUIRE(is_identity_key(support, search_key_for(id_namespaces::anilist)));
}
