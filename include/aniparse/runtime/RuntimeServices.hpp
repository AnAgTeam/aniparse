/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/HttpClient.hpp"
#include "aniparse/runtime/Logger.hpp"
#include "aniparse/cache/ResourceCache.hpp"
#include "aniparse/source/overrides/OverridesState.hpp"
#include <memory>

namespace aniparse {

/**
 * @brief Shared execution services retained by sessions and operation snapshots.
 * The bundle is immutable after use; the services support concurrent access.
 * OverridesStore publishes complete generations. Existing operation snapshots
 * retain their generation while subsequent operations acquire the new one.
 */
struct RuntimeServices {
    std::shared_ptr<HttpClient> client; ///< Required transport.
    std::shared_ptr<Logger> logger; ///< Optional logger.
    std::shared_ptr<ResourceCache> resources = std::make_shared<ResourceCache>(); ///< Shared resource cache.
    std::shared_ptr<OverridesStore> overrides = std::make_shared<OverridesStore>(); ///< Atomic generation publication.
};

} // namespace aniparse
