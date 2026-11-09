#pragma once
#include <aniparse/source/Parser.hpp>
#include <functional>
#include <aniparse/net/CookieStore.hpp>

inline std::shared_ptr<const aniparse::RuntimeServices> test_services(
    std::shared_ptr<const aniparse::RuntimeServices> services) { return services; }

inline std::shared_ptr<aniparse::RuntimeServices> test_services(
    std::shared_ptr<aniparse::HttpClient> client,
    std::shared_ptr<aniparse::Logger> logger = nullptr) {
    auto services = std::make_shared<aniparse::RuntimeServices>();
    services->client = std::move(client);
    services->logger = std::move(logger);
    return services;
}

namespace session_test {
class Factory final : public aniparse::Parser {
public:
    std::string id = "Test";
    aniparse::ProfileInfo profile_info{"main", "Test"};
    std::function<std::unique_ptr<aniparse::ParserProfile>(aniparse::ProfileInfo)> factory;
    aniparse::ParserInfo info() const override { return {.name = id}; }
    std::string identifier() const override { return id; }
    void emplace_domains(aniparse::EmplaceDomainsSink&) const override {}
protected:
    std::vector<aniparse::ProfileInfo> profile_descriptors() const override { return {profile_info}; }
    std::unique_ptr<aniparse::ParserProfile> make_profile_impl(aniparse::ProfileInfo info) const override {
        return factory ? factory(std::move(info)) : std::make_unique<aniparse::ParserProfile>(std::move(info));
    }
};

class ConfiguredProfile final : public aniparse::ParserProfile {
public:
    ConfiguredProfile(aniparse::ProfileInfo info, std::shared_ptr<const aniparse::ParserConfig> defaults)
        : ParserProfile(std::move(info)), defaults_(std::move(defaults)) {}
    void configure(aniparse::ParserConfig& config) const override {
        if (defaults_) config = *defaults_;
    }
private:
    std::shared_ptr<const aniparse::ParserConfig> defaults_;
};
}

template<class Profile = aniparse::ParserProfile>
std::unique_ptr<Profile> test_profile(
    std::string parser_id = "Test", aniparse::ProfileInfo info = {"main", "Test"}) {
    session_test::Factory parser;
    parser.id = std::move(parser_id);
    parser.profile_info = std::move(info);
    parser.factory = [](auto descriptor) { return std::make_unique<Profile>(std::move(descriptor)); };
    auto bound = parser.make_profile(parser.profile_info.id);
    return std::unique_ptr<Profile>(static_cast<Profile*>(bound.release()));
}

inline aniparse::RequestContext test_context(
    std::shared_ptr<const aniparse::RuntimeServices> services,
    std::shared_ptr<const aniparse::ParserConfig> config = nullptr) {
    session_test::Factory parser;
    parser.factory = [config](auto info) { return std::make_unique<session_test::ConfiguredProfile>(std::move(info), config); };
    return parser.make_profile("main")->make_session(std::move(services)).snapshot();
}

inline aniparse::RequestContext test_context(
    std::shared_ptr<aniparse::HttpClient> client,
    std::shared_ptr<aniparse::Logger> logger = nullptr,
    std::shared_ptr<const aniparse::ParserConfig> config = nullptr) {
    return test_context(test_services(std::move(client), std::move(logger)), std::move(config));
}

namespace session_test {
class Client final : public aniparse::HttpClient {
public:
    aniparse::RequestTask<aniparse::HttpResponse> do_request(aniparse::ConfiguredGetRequest) override { co_return aniparse::HttpResponse{}; }
    aniparse::RequestTask<aniparse::HttpResponse> do_request(aniparse::ConfiguredPostRequest) override { co_return aniparse::HttpResponse{}; }
    aniparse::RequestTask<aniparse::HttpResponse> do_request(aniparse::ConfiguredPostMultipartRequest) override { co_return aniparse::HttpResponse{}; }
    void apply_client_config(aniparse::HttpClientConfig) override {}
    std::shared_ptr<aniparse::CookieJar> make_cookie_jar() override { return std::make_shared<aniparse::MemoryCookieJar>(); }
};
}
inline auto test_services() { return test_services(std::make_shared<session_test::Client>()); }
