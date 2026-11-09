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

struct MirrorProfile : ParserProfile {
    using ParserProfile::ParserProfile;
    std::span<const std::string_view> mirrors() const override { return builtin; }
    std::string canonical_base_url() const override { return "https://seed.example"; }
};

auto session_for(std::shared_ptr<const RuntimeServices> services, std::string parser_id, std::string profile_id = "main") {
    return test_profile<MirrorProfile>(std::move(parser_id), {std::move(profile_id), "Test"})->make_session(std::move(services));
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

TEST_CASE("Sessions resolve built-in mirrors and explicit user selection") {
    auto session = session_for(test_services(), "ParserA");
    CHECK(session.mirror_choices() == std::vector<std::string>{"https://a.example", "https://b.example"});
    auto automatic = session.snapshot();
    REQUIRE(session.set_mirror("https://b.example"));
    CHECK(session.snapshot().base_url() == "https://b.example");
    CHECK(automatic.base_url() == "https://a.example");
    REQUIRE(session.set_mirror(std::nullopt));
    CHECK(session.snapshot().base_url() == "https://a.example");
}

TEST_CASE("Override publication changes new snapshots and preserves existing mirror views") {
    auto services = test_services();
    auto session = session_for(services, "ParserA");
    auto builtin_snapshot = session.snapshot();
    services->overrides->store(std::make_shared<const OverridesState>(profile_source(
        {{"ParserA", {"https://old-api.example"}}}, {{"ParserA", "https://old-front.example"}})));
    auto old = session.snapshot();
    auto old_base = old.base_url();
    services->overrides->store(std::make_shared<const OverridesState>(profile_source(
        {{"ParserA", {"https://new-api.example", "https://backup.example"}}}, {{"ParserA", "https://new-front.example"}})));
    auto current = session.snapshot();
    CHECK(builtin_snapshot.base_url() == "https://a.example");
    CHECK(old.base_url() == old_base);
    CHECK(old_base == "https://old-api.example");
    CHECK(old.canonical_base_url() == "https://old-front.example");
    CHECK(current.base_url() == "https://new-api.example");
    CHECK(current.canonical_base_url() == "https://new-front.example");
    CHECK(session.mirror_choices() == std::vector<std::string>{"https://new-api.example", "https://backup.example"});
    auto foreign = session_for(services, "OtherParser").snapshot();
    CHECK(foreign.base_url() == "https://a.example");
    CHECK(foreign.canonical_base_url() == "https://seed.example");
}

TEST_CASE("Profile mirrors and origins do not inherit another profile or extractor overrides", "[profiles]") {
    auto services = test_services();
    services->overrides->store(std::make_shared<const OverridesState>(std::make_shared<const MirrorSource>(
        OverrideMap{{"Provider", {"https://extractor.example"}}},
        MirrorSource::ProfileMirrors{
            {{"Provider", "manga"}, {"https://manga.example"}},
            {{"Provider", "anime"}, {"https://anime.example"}},
        },
        MirrorSource::ProfileOrigins{{{"Provider", "manga"}, "https://manga-front.example"}})));
    auto manga = session_for(services, "Provider", "manga").snapshot();
    CHECK(manga.base_url() == "https://manga.example");
    CHECK(manga.canonical_base_url() == "https://manga-front.example");
    auto anime = session_for(services, "Provider", "anime").snapshot();
    CHECK(anime.base_url() == "https://anime.example");
    CHECK(anime.canonical_base_url() == "https://seed.example");
    auto main = session_for(services, "Provider").snapshot();
    CHECK(main.base_url() == builtin.front());
    CHECK(main.canonical_base_url() == "https://seed.example");
}
