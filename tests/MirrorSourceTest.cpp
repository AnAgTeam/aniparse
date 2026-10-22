/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "TestRequestContext.hpp"
#include "catch_amalgamated.hpp"

#include <aniparse/net/backends/curl/CurlHttpClient.hpp>
#include <aniparse/source/overrides/MirrorSource.hpp>
#include <aniparse/source/Parser.hpp>
#include <aniparse/manga/MangaGetter.hpp>

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
struct NullClient : HttpClient {
	RequestTask<HttpResponse> do_request(ConfiguredGetRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_request_error(RequestErrorCode::NotImplemented, "");
	}
	void apply_client_config(HttpClientConfig) override {}
	std::shared_ptr<CookieJar> make_cookie_jar() override { return nullptr; }
};

// A getter's built-in fallback mirror list.
constexpr std::array builtin = { "https://a.example"sv, "https://b.example"sv };

using OverrideMap = std::map<std::string, std::vector<std::string>, std::less<>>;
using CanonicalBaseMap = std::map<std::string, std::string, std::less<>>;

std::shared_ptr<const MirrorSource> profile_source(OverrideMap mirrors, CanonicalBaseMap origins = {}) {
    MirrorSource::ProfileMirrors profile_mirrors;
    MirrorSource::ProfileOrigins profile_origins;
    for (auto& [id, urls] : mirrors) profile_mirrors.emplace(std::make_pair(id, "main"), std::move(urls));
    for (auto& [id, url] : origins) profile_origins.emplace(std::make_pair(id, "main"), std::move(url));
    return std::make_shared<const MirrorSource>(OverrideMap{}, std::move(profile_mirrors), std::move(profile_origins));
}

std::shared_ptr<MirrorSourceHolder> holder_with(OverrideMap overrides) {
    return std::make_shared<MirrorSourceHolder>(profile_source(std::move(overrides)));
}

RequestContext context_for(std::shared_ptr<const RuntimeServices> services,
                           std::string parser_id, size_t alt_link,
                           std::string profile_id = "main") {
    auto context = ParserProfile(std::move(parser_id), {std::move(profile_id), "Test"}).make_context(services);
    context.set_alt_link(alt_link);
    return context;
}
RequestContext context_for(std::shared_ptr<HttpClient> client,
                           std::string parser_id, size_t alt_link) {
    auto services = std::make_shared<RuntimeServices>();
    services->client = std::move(client);
    return context_for(services, std::move(parser_id), alt_link);
}

// A minimal parser that declares a built-in mirror list, to exercise
// mirror_choices() overlaying a catalog override onto it.
struct MirrorProfile : ParserProfile {
    MirrorProfile() : ParserProfile("MirrorParser", {"main", "Default"}) {}
	std::span<const std::string_view> mirrors() const override {
		static constexpr std::array<std::string_view, 2> urls{ "https://a.example", "https://b.example" };
		return urls;
	}
};

std::vector<std::string> urls_of(const std::vector<AltLink>& links) {
	std::vector<std::string> out;
	for (const AltLink& link : links) {
		out.push_back(link.url);
	}
	return out;
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

TEST_CASE("MirrorSource returns a canonical frontend origin only for a known parser") {
	auto owned_source = profile_source(OverrideMap{}, CanonicalBaseMap{
	    { "ParserA", "https://frontend.example" },
	});
	const auto& source = *owned_source;
	CHECK_FALSE(source.empty_source());
	REQUIRE(source.canonical_base_for("ParserA", "main"));
	CHECK(*source.canonical_base_for("ParserA", "main") == "https://frontend.example");
	CHECK(source.canonical_base_for("Unknown", "main") == nullptr);
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

TEST_CASE("RequestContext base_url falls back to the built-in when no mirror holder") {
	auto client = std::make_shared<NullClient>();
	auto context = context_for(client, "ParserA", 1);

	CHECK(context.base_url(builtin) == "https://b.example"); // alt_link 1
	CHECK(context.mirrors(builtin).count() == 2);
}

TEST_CASE("RequestContext base_url uses the catalog override keyed by parser_id") {
	auto services     = std::make_shared<RuntimeServices>();
	services->client  = std::make_shared<NullClient>();
	services->mirrors = holder_with(OverrideMap{ { "ParserA", { "https://m0.example", "https://m1.example" } } });

	// The parser with an override reads it (selection 1 -> second override mirror).
	auto known = context_for(services, "ParserA", 1);
	CHECK(known.base_url(builtin) == "https://m1.example");
	CHECK(known.mirrors(builtin).count() == 2);

	// A parser with no override falls back to its built-in list.
	auto unknown = context_for(services, "OtherParser", 0);
	CHECK(unknown.base_url(builtin) == "https://a.example");
}

TEST_CASE("RequestContext canonical_base_url uses the catalog override keyed by parser_id") {
	auto services     = std::make_shared<RuntimeServices>();
	services->client  = std::make_shared<NullClient>();
	services->mirrors = std::make_shared<MirrorSourceHolder>(
	    profile_source(OverrideMap{}, CanonicalBaseMap{
	                                                       { "ParserA", "https://frontend.example" },
	                                                   }));

	auto known = context_for(services, "ParserA", 0);
	CHECK(known.canonical_base_url("https://seed.example") == "https://frontend.example");

	auto unknown = context_for(services, "OtherParser", 0);
	CHECK(unknown.canonical_base_url("https://seed.example") == "https://seed.example");
}

TEST_CASE("RequestContext snapshots mirrors at construction, immune to a later swap") {
	auto services     = std::make_shared<RuntimeServices>();
	services->client  = std::make_shared<NullClient>();
	services->mirrors = holder_with(OverrideMap{ { "ParserA", { "https://old.example" } } });

	auto live = context_for(services, "ParserA", 0);
	// Pin a view into the live context's snapshot BEFORE the swap.
	std::string_view pinned = live.base_url(builtin);
	CHECK(pinned == "https://old.example");

	// A catalog apply swaps the holder after the context was built.
	services->mirrors->set(profile_source(OverrideMap{ { "ParserA", { "https://new.example" } } }));

	// The context pins its snapshot by refcount, so the swap does not free the old
	// source: a view obtained earlier is still readable and unchanged.
	CHECK(pinned == "https://old.example");
	// The live context keeps its snapshot; only a freshly built context sees the swap.
	CHECK(live.base_url(builtin) == "https://old.example");
	CHECK(live.new_with_config(live.config()).base_url(builtin) == "https://old.example");
	CHECK(live.new_with_logger(nullptr).base_url(builtin) == "https://old.example");
	auto fresh = context_for(services, "ParserA", 0);
	CHECK(fresh.base_url(builtin) == "https://new.example");
}

TEST_CASE("RequestContext snapshots canonical frontend origins at construction") {
	auto services     = std::make_shared<RuntimeServices>();
	services->client  = std::make_shared<NullClient>();
	services->mirrors = std::make_shared<MirrorSourceHolder>(
	    profile_source(OverrideMap{}, CanonicalBaseMap{
	                                                       { "ParserA", "https://old.example" },
	                                                   }));

	auto live = context_for(services, "ParserA", 0);
	std::string_view pinned = live.canonical_base_url("https://seed.example");
	CHECK(pinned == "https://old.example");

	services->mirrors->set(profile_source(OverrideMap{}, CanonicalBaseMap{ { "ParserA", "https://new.example" } }));

	CHECK(pinned == "https://old.example");
	CHECK(live.canonical_base_url("https://seed.example") == "https://old.example");
	auto fresh = context_for(services, "ParserA", 0);
	CHECK(fresh.canonical_base_url("https://seed.example") == "https://new.example");
}

TEST_CASE("Parser::mirror_choices returns the built-in mirrors without a catalog override") {
	MirrorProfile parser;
	auto client = std::make_shared<NullClient>();
	auto context = context_for(client, "MirrorParser", 0);

	CHECK(urls_of(parser.mirror_choices(context)) ==
	      std::vector<std::string>{ "https://a.example", "https://b.example" });
}

TEST_CASE("Parser::mirror_choices reflects a catalog override keyed by parser id") {
	auto services     = std::make_shared<RuntimeServices>();
	services->client  = std::make_shared<NullClient>();
	services->mirrors = holder_with(OverrideMap{
	    { "MirrorParser", { "https://live1.example", "https://live2.example", "https://live3.example" } } });

	MirrorProfile parser;
	auto context = context_for(services, "MirrorParser", 0);

	// The picker now shows the live (overridden) mirrors, not the built-in list.
	CHECK(urls_of(parser.mirror_choices(context)) ==
	      std::vector<std::string>{ "https://live1.example", "https://live2.example", "https://live3.example" });
}

TEST_CASE("Profile mirrors and origins do not inherit another profile's overrides", "[profiles]") {
    auto services = std::make_shared<RuntimeServices>();
    services->client = std::make_shared<NullClient>();
    services->mirrors = std::make_shared<MirrorSourceHolder>(
        std::make_shared<const MirrorSource>(
            OverrideMap{{"Provider", {"https://legacy.example"}}},
            MirrorSource::ProfileMirrors{
                {{"Provider", "manga"}, {"https://manga.example"}},
                {{"Provider", "anime"}, {"https://anime.example"}},
            },
            MirrorSource::ProfileOrigins{
                {{"Provider", "manga"}, "https://manga-front.example"},
            }));
    auto manga = context_for(services, "Provider", 0, "manga");
    CHECK(manga.base_url(builtin) == "https://manga.example");
    CHECK(manga.canonical_base_url("https://seed.example") == "https://manga-front.example");
    auto anime = context_for(services, "Provider", 0, "anime");
    CHECK(anime.base_url(builtin) == "https://anime.example");
    CHECK(anime.canonical_base_url("https://seed.example") == "https://seed.example");
    auto main = context_for(services, "Provider", 0, "main");
    CHECK(main.base_url(builtin) == builtin.front());
    CHECK(main.canonical_base_url("https://seed.example") == "https://seed.example");
}
