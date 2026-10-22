/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#include "aniparse/net/HttpClient.hpp"

namespace aniparse {
void HttpClient::set_config(HttpClientConfig config) {
    auto defaults = std::make_shared<const RequestDefaults>(config.request_defaults);
    apply_client_config(std::move(config));
    request_defaults_.store(std::move(defaults));
}

RequestDefaults HttpClient::request_defaults() const {
    auto snapshot = request_defaults_.load();
    return snapshot ? *snapshot : RequestDefaults{};
}

} // namespace aniparse
