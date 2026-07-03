/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/ClientContext.hpp"
#include "aniparse/Common.hpp"

namespace aniparse {
struct ImageContainerGetter {
	virtual ~ImageContainerGetter() = default;

	virtual asyncnet::NetworkTask<std::string> title();

	virtual asyncnet::NetworkTask<std::string> description();

	virtual asyncnet::NetworkTask<std::vector<std::string>> tags();

	virtual bool update_client(std::shared_ptr<ClientContext> new_client);

protected:
	std::shared_ptr<ClientContext> client;
};

struct ImagesGetter {
	virtual ~ImagesGetter() = default;

	virtual asyncnet::NetworkTask<PageResults<std::unique_ptr<ImageContainerGetter>>> search(
	    std::shared_ptr<ClientContext> client,
	    std::string query, GetFilters filters);

	virtual asyncnet::NetworkTask<PageResults<std::unique_ptr<ImageContainerGetter>>> latest(
	    std::shared_ptr<ClientContext> client, GetFilters filters);

protected:
	GetterContext context;
};
} // namespace aniparse