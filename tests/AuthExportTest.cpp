/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include <aniparse/Parser.hpp>
#include <aniparse/ClientContext.hpp>
#include <aniparse/types/Authentication.hpp>

#include <memory>

using namespace aniparse;

namespace {

// Minimal vector-backed jar so the test exercises the export/import logic
// without pulling in the curl-backed CurlCookieJar (which needs a live client).
class FakeCookieJar : public CookieJar {
public:
	std::optional<Cookie> find_cookie(std::string_view name) const override {
		for (const Cookie& cookie : cookies_) {
			if (cookie.name == name) {
				return cookie;
			}
		}
		return std::nullopt;
	}

	std::vector<Cookie> cookies() const override {
		return cookies_;
	}

	void set_cookie(const Cookie& cookie) override {
		for (Cookie& existing : cookies_) {
			if (existing.name == cookie.name && existing.domain == cookie.domain &&
			    existing.path == cookie.path) {
				existing = cookie;
				return;
			}
		}
		cookies_.push_back(cookie);
	}

	void clear() override {
		cookies_.clear();
	}

	std::vector<std::string> serialize() const override {
		return {};
	}

	void deserialize(std::span<std::string>) override {
	}

private:
	std::vector<Cookie> cookies_;
};

// Implements only the pure-virtual surface; keeps the default auth_keys()
// (empty) and default export_auth().
struct StubParser : Parser {
	ParserInfo info() const override {
		return { .name = "stub" };
	}
	std::string identifier() const override {
		return "stub";
	}
	ParserCompatibilities compatibilities() const override {
		return {};
	}
	void emplace_domains(EmplaceDomainsContext&) const override {
	}
};

// Declares exactly which cookie / header / url-param entries are the durable
// credential, so export_auth() lifts those and leaves the volatile rest.
struct CredentialParser : StubParser {
	AuthKeys auth_keys() const noexcept override {
		AuthKeys keys;
		keys.cookies    = {"session"};
		keys.headers    = {"Authorization"};
		keys.url_params = {"token"};
		return keys;
	}
};

Cookie make_cookie(std::string name, std::string value) {
	Cookie cookie;
	cookie.name   = std::move(name);
	cookie.value  = std::move(value);
	cookie.domain = "example.com";
	return cookie;
}

ParserConfig make_authed_config() {
	ParserConfig config;
	config.cookie_jar = std::make_shared<FakeCookieJar>();
	config.cookie_jar->set_cookie(make_cookie("session", "secret"));
	config.cookie_jar->set_cookie(make_cookie("antibot", "volatile"));
	config.headers.set("Authorization", "Bearer xyz");
	config.headers.set("User-Agent", "volatile-agent");
	config.url_params["token"]      = "kept";
	config.url_params["ts"]         = "dropped";
	config.alt_link                 = 3;
	return config;
}

} // namespace

TEST_CASE("export_auth keeps only the credential-bearing entries") {
	CredentialParser parser;
	ParserConfig config = make_authed_config();

	AuthState state = parser.export_auth(config);

	REQUIRE(state.cookies.size() == 1);
	REQUIRE(state.cookies[0].name == "session");
	REQUIRE(state.cookies[0].value == "secret");

	REQUIRE(state.headers.size() == 1);
	REQUIRE(state.headers.at("Authorization") == "Bearer xyz");

	REQUIRE(state.url_params.size() == 1);
	REQUIRE(state.url_params.at("token") == "kept");

	REQUIRE(state.alt_link == 3);
}

TEST_CASE("export_auth then import_auth round-trips the session into a fresh config") {
	CredentialParser parser;
	ParserConfig source = make_authed_config();
	source.alt_link     = 7;

	AuthState state = parser.export_auth(source);

	ParserConfig restored;
	restored.cookie_jar = std::make_shared<FakeCookieJar>();
	import_auth(restored, state);

	// Only the durable credential crossed over; volatile entries stayed behind.
	REQUIRE(restored.cookie_jar->cookies().size() == 1);
	auto session = restored.cookie_jar->find_cookie("session");
	REQUIRE(session.has_value());
	REQUIRE(session->value == "secret");
	REQUIRE_FALSE(restored.cookie_jar->find_cookie("antibot").has_value());

	REQUIRE(restored.headers.at("Authorization") == "Bearer xyz");
	REQUIRE_FALSE(restored.headers.contains("User-Agent"));

	REQUIRE(restored.url_params.at("token") == "kept");
	REQUIRE_FALSE(restored.url_params.contains("ts"));

	REQUIRE(restored.alt_link == 7);
}

TEST_CASE("export_auth with no declared keys yields an empty session but keeps alt_link") {
	StubParser parser;
	ParserConfig config = make_authed_config();
	config.alt_link     = 5;

	AuthState state = parser.export_auth(config);

	REQUIRE(state.cookies.empty());
	REQUIRE(state.headers.empty());
	REQUIRE(state.url_params.empty());
	REQUIRE(state.alt_link == 5);
}

TEST_CASE("export_auth tolerates a config without a cookie jar") {
	CredentialParser parser;
	ParserConfig config;
	config.headers.set("Authorization", "Bearer xyz");

	AuthState state = parser.export_auth(config);

	REQUIRE(state.cookies.empty());
	REQUIRE(state.headers.at("Authorization") == "Bearer xyz");
}

TEST_CASE("import_auth without a cookie jar skips cookies but restores the rest") {
	AuthState state;
	state.cookies.push_back(make_cookie("session", "secret"));
	state.headers.set("Authorization", "Bearer xyz");
	state.url_params["token"]      = "kept";
	state.alt_link                 = 4;

	ParserConfig config; // no cookie jar provisioned
	import_auth(config, state);

	REQUIRE(config.headers.at("Authorization") == "Bearer xyz");
	REQUIRE(config.url_params.at("token") == "kept");
	REQUIRE(config.alt_link == 4);
}
