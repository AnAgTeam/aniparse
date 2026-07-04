/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <aniparse/types/Model.hpp>

#include <array>
#include <vector>

using namespace aniparse;

TEST_CASE("Rating::from_score normalizes to a 0..10 scale") {
	Rating rating = Rating::from_score(4.0, 5);

	REQUIRE(rating.score() == Catch::Approx(8.0));
	REQUIRE(rating.max_score() == 5);
	REQUIRE_FALSE(rating.raters().has_value());
	REQUIRE_FALSE(rating.distribution().has_value());
}

TEST_CASE("Rating::from_score keeps the raters count when given") {
	Rating rating = Rating::from_score(9.0, 10, 1234);

	REQUIRE(rating.score() == Catch::Approx(9.0));
	REQUIRE(rating.raters() == 1234);
}

TEST_CASE("Rating::from_score guards against a zero max score") {
	Rating rating = Rating::from_score(7.0, 0);

	REQUIRE(rating.score() == Catch::Approx(0.0));
	REQUIRE(rating.max_score() == 0);
}

TEST_CASE("Rating::from_distribution derives score, raters and distribution") {
	std::array<int, 3> votes{1, 2, 3};
	Rating rating = Rating::from_distribution(votes);

	// weighted = 1*1 + 2*2 + 3*3 = 14; score = 14 / 3
	REQUIRE(rating.score() == Catch::Approx(14.0 / 3.0));
	REQUIRE(rating.max_score() == 3);
	REQUIRE(rating.raters() == 6);

	auto dist = rating.distribution();
	REQUIRE(dist.has_value());
	REQUIRE(dist->size() == 3);
	REQUIRE((*dist)[0] == 1);
	REQUIRE((*dist)[1] == 2);
	REQUIRE((*dist)[2] == 3);
}

TEST_CASE("Rating::from_distribution rejects more than ten buckets") {
	std::array<int, 11> too_many{};
	REQUIRE_THROWS_AS(Rating::from_distribution(too_many), std::logic_error);
}

TEST_CASE("AiredStatus::to_enum maps known names and falls back to Other") {
	REQUIRE(AiredStatus{.name = std::string(aired_status_released)}.to_enum() == DefaultAiredStatuses::Released);
	REQUIRE(AiredStatus{.name = std::string(aired_status_ongoing)}.to_enum() == DefaultAiredStatuses::Ongoing);
	REQUIRE(AiredStatus{.name = std::string(aired_status_announced)}.to_enum() == DefaultAiredStatuses::Announced);
	REQUIRE(AiredStatus{.name = "something-else"}.to_enum() == DefaultAiredStatuses::Other);
}

TEST_CASE("UserList::to_enum maps every known name and falls back to Other") {
	REQUIRE(UserList{.name = std::string(user_list_planning)}.to_enum() == DefaultUserLists::Planning);
	REQUIRE(UserList{.name = std::string(user_list_dropped)}.to_enum() == DefaultUserLists::Dropped);
	REQUIRE(UserList{.name = std::string(user_list_favorite)}.to_enum() == DefaultUserLists::Favorite);
	REQUIRE(UserList{.name = std::string(user_list_watched)}.to_enum() == DefaultUserLists::Watched);
	REQUIRE(UserList{.name = std::string(user_list_watching)}.to_enum() == DefaultUserLists::Watching);
	REQUIRE(UserList{.name = std::string(user_list_reading)}.to_enum() == DefaultUserLists::Reading);
	REQUIRE(UserList{.name = std::string(user_list_read)}.to_enum() == DefaultUserLists::Read);
	REQUIRE(UserList{.name = "unknown"}.to_enum() == DefaultUserLists::Other);
}
