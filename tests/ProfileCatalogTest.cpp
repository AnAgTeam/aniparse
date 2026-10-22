/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "catch_amalgamated.hpp"
#include <aniparse/source/overrides/SourceOverridesManager.hpp>

#include <array>
#include <boost/json.hpp>

using namespace aniparse;
namespace {
struct Verifier : SignatureVerifier {
	bool accepted = true;
	mutable std::string verified_payload;
	bool verify(std::string_view payload, std::string_view) const override {
		verified_payload = payload;
		return accepted;
	}
};

struct NoNetwork : HttpClient {
	RequestTask<HttpResponse> do_request(ConfiguredGetRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	RequestTask<HttpResponse> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	void apply_client_config(HttpClientConfig) override {}
	std::shared_ptr<CookieJar> make_cookie_jar() override { return nullptr; }
};

constexpr std::string_view payload = R"({
    "schema_version": 1, "revision": 7,
    "parsers": {"Example": {
        "mirrors": ["https://legacy-api.example"],
        "canonical_base_url": "https://legacy-front.example",
        "profiles": {
            "main": {"mirrors": ["https://main-api.example"],
                     "canonical_base_url": "https://main-front.example"},
            "reader": {"mirrors": ["https://reader-api.example"],
                       "canonical_base_url": "https://reader-front.example"},
            "empty": {}
        }
    }}
})";

constexpr std::array<std::string_view, 1> builtin{"https://builtin-api.example"};
std::shared_ptr<ParserConfig> config(std::string profile, std::string parser = "Example") {
	auto result = std::make_shared<ParserConfig>();
	result->parser_id = std::move(parser);
	result->profile_id = std::move(profile);
	return result;
}
std::shared_ptr<RuntimeServices> services() {
	auto result = std::make_shared<RuntimeServices>();
	result->client = std::make_shared<NoNetwork>();
	result->mirrors = std::make_shared<MirrorSourceHolder>();
	return result;
}
}

TEST_CASE("Signed catalog decodes profiles and ignores obsolete parser-wide origins", "[profile-catalog]") {
	Verifier verifier;
	auto decoded = decode_overrides(payload, "signature", verifier);
	REQUIRE(decoded);
	CHECK(verifier.verified_payload == payload);
	CHECK((*decoded->profiles.at({"Example", "main"}).mirrors).front() == "https://main-api.example");
	CHECK((*decoded->profiles.at({"Example", "reader"}).mirrors).front() == "https://reader-api.example");
	CHECK((*decoded->profiles.at({"Example", "reader"}).canonical_base_url) == "https://reader-front.example");
	CHECK(decoded->profiles.size() == 2);
	CHECK_FALSE(decoded->profiles.contains({"Example", "empty"}));
}

TEST_CASE("Malformed profile fields reject the signed catalog", "[profile-catalog]") {
	Verifier verifier;
	for (auto fields : {
	    R"({"profiles":[]})", R"({"profiles":null})", R"({"profiles":{"":{}}})",
	    R"({"profiles":{"p":[]}})", R"({"profiles":{"p":{"mirrors":42}}})",
	    R"({"profiles":{"p":{"mirrors":[42]}}})", R"({"profiles":{"p":{"mirrors":[""]}}})",
	    R"({"profiles":{"p":{"canonical_base_url":42}}})",
	    R"({"profiles":{"p":{"canonical_base_url":""}}})"}) {
		INFO(fields);
		const auto text = std::string(R"({"schema_version":1,"revision":1,"parsers":{"Example":)") + fields + "}}";
		auto decoded = decode_overrides(text, "signature", verifier);
		REQUIRE_FALSE(decoded);
		CHECK(decoded.error() == OverridesError::BadFormat);
	}
}

TEST_CASE("SourceOverridesManager applies isolated profile settings and keeps old snapshots", "[profile-catalog]") {
	ParserStore store;
	Verifier verifier;
	auto state = services();
	SourceOverridesManager manager(store, verifier, state);
	RequestContext before(state, config("reader"));
	REQUIRE(manager.apply(payload, "signature"));
	RequestContext main(state, config("main"));
	RequestContext reader(state, config("reader"));
	RequestContext other(state, config("other"));
	RequestContext foreign(state, config("reader", "OtherParser"));
	CHECK(before.base_url(builtin) == builtin.front());
	CHECK(main.base_url(builtin) == "https://main-api.example");
	CHECK(main.canonical_base_url("https://builtin-front.example") == "https://main-front.example");
	CHECK(reader.base_url(builtin) == "https://reader-api.example");
	CHECK(reader.canonical_base_url("https://builtin-front.example") == "https://reader-front.example");
	CHECK(other.base_url(builtin) == builtin.front());
	CHECK(other.canonical_base_url("https://builtin-front.example") == "https://builtin-front.example");
	CHECK(foreign.base_url(builtin) == builtin.front());

	auto pinned = state->mirrors->get();
	auto updated = boost::json::parse(payload).as_object();
	updated["revision"] = 8;
	updated.at("parsers").as_object().at("Example").as_object()
	    .at("profiles").as_object().at("reader").as_object()["mirrors"] =
	    boost::json::array{"https://new-reader-api.example"};
	const auto newer = boost::json::serialize(updated);
	verifier.accepted = false;
	CHECK_FALSE(manager.apply(newer, "bad signature"));
	CHECK(state->mirrors->get() == pinned);
	CHECK(manager.revision() == 7);
	verifier.accepted = true;
	REQUIRE(manager.apply(newer, "signature"));
	RequestContext fresh(state, config("reader"));
	CHECK(fresh.base_url(builtin) == "https://new-reader-api.example");
	CHECK(reader.base_url(builtin) == "https://reader-api.example");

	auto malformed = updated;
	malformed["revision"] = 9;
	malformed.at("parsers").as_object().at("Example").as_object()["profiles"] = 42;
	pinned = state->mirrors->get();
	CHECK_FALSE(manager.apply(boost::json::serialize(malformed), "signature"));
	CHECK(state->mirrors->get() == pinned);
	CHECK(manager.revision() == 8);
	manager.reset();
	RequestContext reset(state, config("reader"));
	CHECK(reset.base_url(builtin) == builtin.front());
	CHECK(reset.canonical_base_url("https://builtin-front.example") == "https://builtin-front.example");
	CHECK(fresh.base_url(builtin) == "https://new-reader-api.example");
	CHECK(manager.revision() == 0);
}

TEST_CASE("Obsolete parser-wide origins are ignored and empty profile mirrors use builtins", "[profile-catalog]") {
	ParserStore store;
	Verifier verifier;
	auto state = services();
	SourceOverridesManager manager(store, verifier, state);
	REQUIRE(manager.apply(R"({"schema_version":1,"revision":1,"parsers":{"Example":{
	    "mirrors":["https://legacy.example"],"canonical_base_url":"https://legacy-front.example"}}})", "signature"));
	RequestContext legacy(state, config("main"));
	CHECK(legacy.base_url(builtin) == builtin.front());
	CHECK(legacy.canonical_base_url("https://builtin-front.example") == "https://builtin-front.example");
	REQUIRE(manager.apply(R"({"schema_version":1,"revision":2,"parsers":{"Example":{
	    "mirrors":["https://legacy.example"],"profiles":{"main":{"mirrors":[]}}}}})", "signature"));
	RequestContext cleared(state, config("main"));
	CHECK(cleared.base_url(builtin) == builtin.front());
	CHECK(legacy.base_url(builtin) == builtin.front());
	RequestContext unstamped(state, config(""));
	CHECK(unstamped.base_url(builtin) == builtin.front());
	CHECK(unstamped.canonical_base_url("https://builtin-front.example") == "https://builtin-front.example");
}

TEST_CASE("Profile overrides preserve independent absent and empty channels", "[profile-catalog]") {
    Verifier verifier;
    const auto text = R"({"schema_version":1,"revision":1,"parsers":{"Example":{"profiles":{
        "mirrors-only":{"mirrors":[]},
        "origin-only":{"canonical_base_url":"https://frontend.example"}
    }}}})";
    auto decoded = decode_overrides(text, "signature", verifier);
    REQUIRE(decoded);
    const auto& mirrors = decoded->profiles.at({"Example", "mirrors-only"});
    REQUIRE(mirrors.mirrors);
    CHECK(mirrors.mirrors->empty());
    CHECK_FALSE(mirrors.canonical_base_url);
    const auto& origin = decoded->profiles.at({"Example", "origin-only"});
    CHECK_FALSE(origin.mirrors);
    CHECK(origin.canonical_base_url == "https://frontend.example");
}
