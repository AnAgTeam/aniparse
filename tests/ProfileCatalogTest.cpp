/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "catch_amalgamated.hpp"
#include <aniparse/catalog/CatalogManager.hpp>

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

struct NoNetwork : ClientContext {
	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest) override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "");
	}
	void apply_client_config(ClientConfig) override {}
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
std::shared_ptr<ServiceState> services() {
	auto result = std::make_shared<ServiceState>();
	result->client = std::make_shared<NoNetwork>();
	result->mirrors = std::make_shared<MirrorSourceHolder>();
	return result;
}
}

TEST_CASE("Signed catalog decodes profiles and ignores obsolete parser-wide origins", "[profile-catalog]") {
	Verifier verifier;
	auto decoded = decode_catalog(payload, "signature", verifier);
	REQUIRE(decoded);
	CHECK(verifier.verified_payload == payload);
	CHECK(decoded->profile_mirrors.at({"Example", "main"}).front() == "https://main-api.example");
	CHECK(decoded->profile_mirrors.at({"Example", "reader"}).front() == "https://reader-api.example");
	CHECK(decoded->profile_canonical_base_urls.at({"Example", "reader"}) == "https://reader-front.example");
	CHECK(decoded->profile_mirrors.size() == 2);
	CHECK(decoded->profile_canonical_base_urls.size() == 2);
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
		auto decoded = decode_catalog(text, "signature", verifier);
		REQUIRE_FALSE(decoded);
		CHECK(decoded.error() == CatalogError::BadFormat);
	}
}

TEST_CASE("CatalogManager applies isolated profile settings and keeps old snapshots", "[profile-catalog]") {
	ParserStore store;
	Verifier verifier;
	auto state = services();
	CatalogManager manager(store, verifier, state);
	RequestorContext before(state, config("reader"));
	REQUIRE(manager.apply(payload, "signature"));
	RequestorContext main(state, config("main"));
	RequestorContext reader(state, config("reader"));
	RequestorContext other(state, config("other"));
	RequestorContext foreign(state, config("reader", "OtherParser"));
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
	RequestorContext fresh(state, config("reader"));
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
	RequestorContext reset(state, config("reader"));
	CHECK(reset.base_url(builtin) == builtin.front());
	CHECK(reset.canonical_base_url("https://builtin-front.example") == "https://builtin-front.example");
	CHECK(fresh.base_url(builtin) == "https://new-reader-api.example");
	CHECK(manager.revision() == 0);
}

TEST_CASE("Obsolete parser-wide origins are ignored and empty profile mirrors use builtins", "[profile-catalog]") {
	ParserStore store;
	Verifier verifier;
	auto state = services();
	CatalogManager manager(store, verifier, state);
	REQUIRE(manager.apply(R"({"schema_version":1,"revision":1,"parsers":{"Example":{
	    "mirrors":["https://legacy.example"],"canonical_base_url":"https://legacy-front.example"}}})", "signature"));
	RequestorContext legacy(state, config("main"));
	CHECK(legacy.base_url(builtin) == builtin.front());
	CHECK(legacy.canonical_base_url("https://builtin-front.example") == "https://builtin-front.example");
	REQUIRE(manager.apply(R"({"schema_version":1,"revision":2,"parsers":{"Example":{
	    "mirrors":["https://legacy.example"],"profiles":{"main":{"mirrors":[]}}}}})", "signature"));
	RequestorContext cleared(state, config("main"));
	CHECK(cleared.base_url(builtin) == builtin.front());
	CHECK(legacy.base_url(builtin) == builtin.front());
	RequestorContext unstamped(state, config(""));
	CHECK(unstamped.base_url(builtin) == builtin.front());
	CHECK(unstamped.canonical_base_url("https://builtin-front.example") == "https://builtin-front.example");
}
