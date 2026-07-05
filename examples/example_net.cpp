/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
// example_net — the networking half of the toolkit. It makes real HTTP requests
// through the default curl-backed client and shows the three ways a parser
// reaches the network:
//   - context.request()      -> raw ResponseData (status, headers, effective_url, body)
//   - context.request_html() -> a parsed HTML document (fetch + status check + parse)
//   - context.request_json() -> a parsed JSON value
// Everything above the client is backend-neutral: swap AsyncClient for another
// ClientContext (e.g. an NSURLSession-backed one) and this code is unchanged.
//
// Unlike example_parse this one needs network access; each call reports its own
// error through the expected channel, so a failure just prints and moves on.
#include <aniparse/Client.hpp>
#include <aniparse/ClientContext.hpp>
#include <aniparse/html/HTMLDocument.hpp>

#include <boost/json.hpp>
#include <coro/sync_wait.hpp>

#include <print>

using namespace aniparse;

int main() {
	// The default client is curl-backed. Wrapping it in a RequestorContext is all
	// a parser ever sees; no logger or config is needed for a plain request.
	auto client = std::make_shared<AsyncClient>();
	RequestorContext context(client, nullptr, nullptr);

	// 1. Raw request: the escape hatch for the low-level response. Note that the
	//    status, response headers and effective_url are all available here.
	std::println("GET https://example.com");
	if (auto response = coro::sync_wait(context.request(GetRequest{ .url = "https://example.com" }))) {
		std::println("  status:        {}", response->status_code);
		std::println("  effective_url: {}", response->effective_url.value_or("(none)"));
		if (auto header = response->headers.find("content-type"); header != response->headers.end()) {
			std::println("  content-type:  {}", header->second);
		}
		std::println("  body bytes:    {}", response->body.size());
	} else {
		std::println("  failed: {}", response.error().message);
	}

	// 2. request_html: fetch + 2xx check + parse, all folded into one expected.
	std::println("\nrequest_html https://example.com");
	if (auto document = coro::sync_wait(context.request_html(GetRequest{ .url = "https://example.com" }))) {
		std::println("  <title>: {}", document->title());
	} else {
		std::println("  failed: {}", document.error().message);
	}

	// 3. request_json: same, but the body is parsed as JSON.
	std::println("\nrequest_json https://jsonplaceholder.typicode.com/users/1");
	if (auto json = coro::sync_wait(context.request_json(GetRequest{ .url = "https://jsonplaceholder.typicode.com/users/1" }))) {
		if (json->is_object()) {
			const boost::json::object& user = json->as_object();
			if (auto name = user.if_contains("name"); name && name->is_string()) {
				std::println("  user name: {}", name->as_string().c_str());
			}
		}
	} else {
		std::println("  failed: {}", json.error().message);
	}

	return 0;
}
