/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"
#include "CoroTest.hpp"

#include <aniparse/media/ResourceAdapter.hpp>

#include <array>
#include <string>
#include <vector>

namespace {

class CollectingOutput final : public aniparse::ResourceOutput {
public:
	aniparse::Response<void> write(std::span<const std::byte> bytes) override {
		bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
		return {};
	}

	[[nodiscard]] const std::vector<std::byte>& bytes() const noexcept {
		return bytes_;
	}

private:
	std::vector<std::byte> bytes_;
};

class NullClient final : public aniparse::ClientContext {
public:
	aniparse::NetworkRequestTask<aniparse::ResponseData> do_request(aniparse::ConfiguredGetRequest) override {
		co_return aniparse::make_response_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	aniparse::NetworkRequestTask<aniparse::ResponseData> do_request(aniparse::ConfiguredPostRequest) override {
		co_return aniparse::make_response_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	aniparse::NetworkRequestTask<aniparse::ResponseData> do_request(
	    aniparse::ConfiguredPostMultipartRequest) override {
		co_return aniparse::make_response_error(aniparse::RequestErrorCode::NotImplemented, "");
	}
	void set_config(aniparse::ClientConfig) override {}
	std::shared_ptr<aniparse::CookieJar> make_cookie_jar() override { return nullptr; }
};

aniparse::RequestorContext make_context() {
	return aniparse::RequestorContext(std::make_shared<NullClient>(), nullptr,
	                                  std::make_shared<aniparse::ParserConfig>());
}

class SuffixHandle final : public aniparse::ResourceHandle {
public:
	explicit SuffixHandle(std::byte suffix) : suffix_(suffix) {}

	aniparse::Response<void> update(std::span<const std::byte> input,
	                                aniparse::ResourceOutput& output) override {
		auto copied = output.write(input);
		if (!copied) {
			return copied;
		}
		return output.write(std::span(&suffix_, 1));
	}

private:
	std::byte suffix_;
};

class TestAdapter final : public aniparse::ResourceAdapter {
public:
	TestAdapter(std::string request_suffix, std::byte response_suffix)
	    : request_suffix_(std::move(request_suffix)), response_suffix_(response_suffix) {}

	aniparse::NetworkRequestTask<aniparse::OpenedResource> open(
	    aniparse::RequestorContext,
	    aniparse::ResourceContext resource) const override {
		resource.resource.url += request_suffix_;
		resource.resource.headers.set("X-Test", request_suffix_);
		co_return aniparse::OpenedResource{
		    .request = {
		        .url = std::move(resource.resource.url),
		        .headers = std::move(resource.resource.headers),
		    },
		    .handle = std::make_unique<SuffixHandle>(response_suffix_),
		};
	}

private:
	std::string request_suffix_;
	std::byte response_suffix_;
};

} // namespace

TEST_CASE("ResourceHandle defaults preserve metadata and pass through bytes", "[resource-adapter]") {
	aniparse::ResourceHandle handle;
	aniparse::ResourceMetadata metadata{
	    .status_code = 200,
	    .content_type = "application/octet-stream",
	};
	CollectingOutput output;
	const std::array input{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};

	REQUIRE(handle.on_response(metadata));
	REQUIRE(handle.update(input, output));
	REQUIRE(handle.final(output));
	CHECK(metadata.content_type == "application/octet-stream");
	CHECK(output.bytes() == std::vector<std::byte>(input.begin(), input.end()));
}

CORO_TEST_CASE("ResourceAdapter composes nested requests and reverse response transforms", "[resource-adapter]") {
	using namespace aniparse;
	auto outer = std::make_shared<TestAdapter>("/outer", std::byte{'o'});
	auto inner = std::make_shared<TestAdapter>("/inner", std::byte{'i'});
	auto adapter = ResourceAdapter::compose({ outer, inner });
	REQUIRE(adapter);

	auto opened = co_await adapter->open(make_context(), {
	    .resource = {
	        .url = "https://video.example/stream",
	    },
	});
	REQUIRE(opened);
	CHECK(opened->request.url == "https://video.example/stream/outer/inner");
	CHECK(opened->request.headers.get("X-Test") == "/inner");
	REQUIRE(opened->handle);

	CollectingOutput output;
	const std::array input{std::byte{'x'}};
	REQUIRE(opened->handle->update(input, output));
	REQUIRE(opened->handle->final(output));
	CHECK(output.bytes() == std::vector<std::byte>{
	    std::byte{'x'}, std::byte{'o'}, std::byte{'i'}, std::byte{'o'},
	});
}
