/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/types/Pagination.hpp>

#include <memory>
#include <string>

using namespace aniparse;

TEST_CASE("clamp_limit resolves an unset limit to the fallback", "[pagination]") {
	GetFilters filters;                 // limit defaults to page_no_limit
	REQUIRE(clamp_limit(filters, 200, 20) == 20);
}

TEST_CASE("clamp_limit caps a requested limit", "[pagination]") {
	GetFilters filters;
	filters.limit = 500;
	REQUIRE(clamp_limit(filters, 200, 20) == 200);

	filters.limit = 50;
	REQUIRE(clamp_limit(filters, 200, 20) == 50);

	filters.limit = 200;
	REQUIRE(clamp_limit(filters, 200, 20) == 200);
}

TEST_CASE("OffsetPaging maps an item offset to a page and intra-page skip", "[pagination]") {
	// perPage 30: offset 0 -> page 1 skip 0; offset 45 -> page 2 skip 15.
	const OffsetPaging first{ .from = 0, .stride = 30 };
	REQUIRE(first.page() == 1);
	REQUIRE(first.skip() == 0);

	const OffsetPaging mid{ .from = 45, .stride = 30 };
	REQUIRE(mid.page() == 2);
	REQUIRE(mid.skip() == 15);

	// A page boundary lands at skip 0 on the next page (60/30 + 1 == 3).
	const OffsetPaging boundary{ .from = 60, .stride = 30 };
	REQUIRE(boundary.page() == 3);
	REQUIRE(boundary.skip() == 0);
}

TEST_CASE("OffsetPaging honours a custom page base and a zero stride", "[pagination]") {
	const OffsetPaging zero_based{ .from = 40, .stride = 20 };
	REQUIRE(zero_based.page(0) == 2);

	// A zero stride cannot divide; page falls back to base and skip to 0.
	const OffsetPaging degenerate{ .from = 99, .stride = 0 };
	REQUIRE(degenerate.page() == 1);
	REQUIRE(degenerate.skip() == 0);
}

TEST_CASE("PageResults::append numbers items from the absolute offset", "[pagination]") {
	PageResults<std::string> page;
	page.append(100, "a");
	page.append(100, "b");
	page.append(100, "c");

	REQUIRE(page.results.size() == 3);
	REQUIRE(page.results[0].offset == 100);
	REQUIRE(page.results[1].offset == 101);
	REQUIRE(page.results[2].offset == 102);
	REQUIRE(page.results[0].item == "a");
	REQUIRE(page.results[2].item == "c");
	// next_offset points just past the last item.
	REQUIRE(page.next_offset == 103);
}

TEST_CASE("PageResults::append moves move-only items", "[pagination]") {
	PageResults<std::unique_ptr<int>> page;
	page.append(0, std::make_unique<int>(7));
	page.append(0, std::make_unique<int>(8));

	REQUIRE(page.results.size() == 2);
	REQUIRE(*page.results[0].item == 7);
	REQUIRE(page.results[0].offset == 0);
	REQUIRE(page.results[1].offset == 1);
	REQUIRE(page.next_offset == 2);
}
