/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "catch_amalgamated.hpp"

#include <aniparse/media/ResourceAdapter.hpp>

#include <array>
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
