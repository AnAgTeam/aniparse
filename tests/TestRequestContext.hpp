#pragma once
#include <aniparse/source/ParserProfile.hpp>

// Generic configured profile for tests of runtime behavior, independent of a source.
inline aniparse::RequestContext test_context(
    std::shared_ptr<const aniparse::RuntimeServices> services,
    std::shared_ptr<const aniparse::ParserConfig> config = nullptr) {
    aniparse::ParserProfile profile("Test", {"main", "Test"});
    auto context = profile.make_context(std::move(services));
    return config ? context.new_with_config(std::move(config)) : context;
}

inline aniparse::RequestContext test_context(
    std::shared_ptr<aniparse::HttpClient> client,
    std::shared_ptr<aniparse::Logger> logger = nullptr,
    std::shared_ptr<const aniparse::ParserConfig> config = nullptr) {
    auto services = std::make_shared<aniparse::RuntimeServices>();
    services->client = std::move(client);
    services->logger = std::move(logger);
    return test_context(std::move(services), std::move(config));
}
