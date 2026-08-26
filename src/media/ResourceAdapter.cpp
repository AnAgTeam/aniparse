/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/media/ResourceAdapter.hpp"

#include <algorithm>
#include <utility>

namespace aniparse {
namespace {

class ChainedResourceHandle final : public ResourceHandle {
public:
	explicit ChainedResourceHandle(std::vector<std::unique_ptr<ResourceHandle>> handles)
	    : handles_(std::move(handles)) {}

	Response<void> on_response(ResourceMetadata& metadata) override {
		for (auto handle = handles_.rbegin(); handle != handles_.rend(); ++handle) {
			auto result = (*handle)->on_response(metadata);
			if (!result) {
				return unexpected(std::move(result.error()));
			}
		}
		return {};
	}

	Response<void> update(std::span<const std::byte> input, ResourceOutput& output) override {
		return update_prefix(handles_.size(), input, output);
	}

	Response<void> final(ResourceOutput& output) override {
		for (std::size_t count = handles_.size(); count > 0; --count) {
			ForwardingOutput forwarded(*this, count - 1, output);
			auto result = handles_[count - 1]->final(forwarded);
			if (!result) {
				return unexpected(std::move(result.error()));
			}
		}
		return {};
	}

private:
	class ForwardingOutput final : public ResourceOutput {
	public:
		ForwardingOutput(ChainedResourceHandle& chain, std::size_t prefix_count,
		                 ResourceOutput& output) noexcept
		    : chain_(chain), prefix_count_(prefix_count), output_(output) {}

		Response<void> write(std::span<const std::byte> bytes) override {
			return chain_.update_prefix(prefix_count_, bytes, output_);
		}

	private:
		ChainedResourceHandle& chain_;
		std::size_t prefix_count_;
		ResourceOutput& output_;
	};

	Response<void> update_prefix(std::size_t count, std::span<const std::byte> input,
	                             ResourceOutput& output) {
		if (count == 0) {
			return output.write(input);
		}
		ForwardingOutput forwarded(*this, count - 1, output);
		return handles_[count - 1]->update(input, forwarded);
	}

	std::vector<std::unique_ptr<ResourceHandle>> handles_;
};

class ComposedResourceAdapter final : public ResourceAdapter {
public:
	explicit ComposedResourceAdapter(std::vector<std::shared_ptr<const ResourceAdapter>> adapters)
	    : adapters_(std::move(adapters)) {}

	NetworkRequestTask<OpenedResource> open(RequestorContext context, ResourceContext resource) const override {
		ResourceContext current = std::move(resource);
		std::vector<std::unique_ptr<ResourceHandle>> handles;
		for (const std::shared_ptr<const ResourceAdapter>& adapter : adapters_) {
			auto opened = co_await adapter->open(context, std::move(current));
			if (!opened) {
				co_return unexpected(std::move(opened.error()));
			}
			current.resource.url = opened->request.url;
			current.resource.headers = opened->request.headers;
			if (opened->handle) {
				handles.push_back(std::move(opened->handle));
			}
		}

		OpenedResource result;
		result.request = {
			.url = std::move(current.resource.url),
			.headers = std::move(current.resource.headers),
		};
		if (handles.size() == 1) {
			result.handle = std::move(handles.front());
		} else if (!handles.empty()) {
			result.handle = std::make_unique<ChainedResourceHandle>(std::move(handles));
		}
		co_return result;
	}

private:
	std::vector<std::shared_ptr<const ResourceAdapter>> adapters_;
};

} // namespace

std::shared_ptr<const ResourceAdapter> ResourceAdapter::compose(
    std::vector<std::shared_ptr<const ResourceAdapter>> adapters) {
	std::erase(adapters, nullptr);
	if (adapters.empty()) {
		return nullptr;
	}
	if (adapters.size() == 1) {
		return std::move(adapters.front());
	}
	return std::make_shared<const ComposedResourceAdapter>(std::move(adapters));
}

} // namespace aniparse
