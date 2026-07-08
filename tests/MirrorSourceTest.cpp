/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/Client.hpp>
#include <aniparse/MirrorSource.hpp>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace aniparse;
using namespace std::string_view_literals;

namespace {
// The context resolves base URLs without touching the network, so this client
// only needs to exist; its request paths are never taken.
struct NullClient : ClientContext {
	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	void set_config(ClientConfig) override {}
	std::shared_ptr<CookieJar> make_cookie_jar() override { return nullptr; }
};

// A getter's built-in fallback mirror list.
constexpr std::array builtin = { "https://a.example"sv, "https://b.example"sv };

using OverrideMap = std::map<std::string, std::vector<std::string>, std::less<>>;

std::shared_ptr<MirrorSourceHolder> holder_with(OverrideMap overrides) {
	return std::make_shared<MirrorSourceHolder>(
	    std::make_shared<const MirrorSource>(std::move(overrides)));
}

std::shared_ptr<ParserConfig> config_for(std::string parser_id, size_t alt_link) {
	auto config       = std::make_shared<ParserConfig>();
	config->parser_id = std::move(parser_id);
	config->alt_link  = alt_link;
	return config;
}
} // namespace

TEST_CASE("Mirrors falls back to the built-in list when there is no override") {
	Mirrors m{ nullptr, builtin };
	CHECK(m.count() == 2);
	CHECK(m.base_url(0) == "https://a.example");
	CHECK(m.base_url(1) == "https://b.example");
	// An out-of-range selection clamps to the first mirror.
	CHECK(m.base_url(5) == "https://a.example");
}

TEST_CASE("Mirrors prefers a non-empty override over the built-in list") {
	std::vector<std::string> override_list = { "https://m0.example", "https://m1.example", "https://m2.example" };
	Mirrors m{ &override_list, builtin };
	CHECK(m.count() == 3);
	CHECK(m.base_url(2) == "https://m2.example");
	CHECK(m.base_url(9) == "https://m0.example"); // clamp
}

TEST_CASE("Mirrors treats an empty override as a fall-through to the built-in list") {
	std::vector<std::string> empty_override;
	Mirrors m{ &empty_override, builtin };
	// count() and base_url() agree: an empty override behaves like no override.
	CHECK(m.count() == 2);
	CHECK(m.base_url(1) == "https://b.example");
}

TEST_CASE("Mirrors with neither override nor built-in yields nothing") {
	Mirrors m{ nullptr, {} };
	CHECK(m.count() == 0);
	CHECK(m.base_url(0).empty());
}

TEST_CASE("MirrorSource returns an override list only for a known parser") {
	MirrorSource source(OverrideMap{
	    { "ParserA", { "https://x.example" } },
	    { "ParserB", { "https://y.example", "https://z.example" } },
	});
	CHECK_FALSE(source.empty_source());
	REQUIRE(source.list_for("ParserA"));
	CHECK(*source.list_for("ParserA") == std::vector<std::string>{ "https://x.example" });
	CHECK(source.list_for("Unknown") == nullptr);

	CHECK(MirrorSource{}.empty_source());
}

TEST_CASE("MirrorSourceHolder defaults to a non-null empty source and swaps") {
	MirrorSourceHolder holder;
	REQUIRE(holder.get());
	CHECK(holder.get()->empty_source());

	holder.set(std::make_shared<const MirrorSource>(OverrideMap{ { "ParserA", { "https://o.example" } } }));
	REQUIRE(holder.get()->list_for("ParserA"));

	// A null swap becomes an empty source, never null.
	holder.set(nullptr);
	REQUIRE(holder.get());
	CHECK(holder.get()->empty_source());
}

TEST_CASE("RequestorContext base_url falls back to the built-in when no mirror holder") {
	auto client = std::make_shared<NullClient>();
	RequestorContext context(client, nullptr, config_for("ParserA", 1));

	CHECK(context.base_url(builtin) == "https://b.example"); // alt_link 1
	CHECK(context.mirrors(builtin).count() == 2);
}

TEST_CASE("RequestorContext base_url uses the catalog override keyed by parser_id") {
	auto services     = std::make_shared<ServiceState>();
	services->client  = std::make_shared<NullClient>();
	services->mirrors = holder_with(OverrideMap{ { "ParserA", { "https://m0.example", "https://m1.example" } } });

	// The parser with an override reads it (selection 1 -> second override mirror).
	RequestorContext known(services, config_for("ParserA", 1));
	CHECK(known.base_url(builtin) == "https://m1.example");
	CHECK(known.mirrors(builtin).count() == 2);

	// A parser with no override falls back to its built-in list.
	RequestorContext unknown(services, config_for("OtherParser", 0));
	CHECK(unknown.base_url(builtin) == "https://a.example");
}

TEST_CASE("RequestorContext snapshots mirrors at construction, immune to a later swap") {
	auto services     = std::make_shared<ServiceState>();
	services->client  = std::make_shared<NullClient>();
	services->mirrors = holder_with(OverrideMap{ { "ParserA", { "https://old.example" } } });

	RequestorContext live(services, config_for("ParserA", 0));
	// Pin a view into the live context's snapshot BEFORE the swap.
	std::string_view pinned = live.base_url(builtin);
	CHECK(pinned == "https://old.example");

	// A catalog apply swaps the holder after the context was built.
	services->mirrors->set(std::make_shared<const MirrorSource>(
	    OverrideMap{ { "ParserA", { "https://new.example" } } }));

	// The context pins its snapshot by refcount, so the swap does not free the old
	// source: a view obtained earlier is still readable and unchanged.
	CHECK(pinned == "https://old.example");
	// The live context keeps its snapshot; only a freshly built context sees the swap.
	CHECK(live.base_url(builtin) == "https://old.example");
	RequestorContext fresh(services, config_for("ParserA", 0));
	CHECK(fresh.base_url(builtin) == "https://new.example");
}
