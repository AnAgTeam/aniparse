#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Format.hpp"
#include <ranges>

namespace aniparse {

    static void apply_config_to(auto& request, const ClientConfig& config) {
        auto joined_headers = config.headers | std::views::transform([](auto& header) {
            return header.first + ": " + header.second;
        });

        std::ranges::copy(joined_headers, std::back_inserter(request.headers));

        // UrlParameters doesn't have an iterator
        //std::ranges::copy(config.url_params, std::back_inserter(request.url_params));

        for (auto& header : config.url_params) {
            request.url_params += header;
        }
    }

    RequestorContext::RequestorContext(std::shared_ptr<ClientContext> client,
        std::shared_ptr<LoggerContext> logger,
        std::shared_ptr<ClientConfig> config)
        : client_(std::move(client))
        , logger_(std::move(logger))
        , config_(config ? std::move(config) : std::make_shared<ClientConfig>()) {
        if (!client_) {
            throw std::invalid_argument("Client has to be valid");
        }
    }

    asyncnet::NetworkTask<asyncnet::Response> RequestorContext::request(GetRequest request) {
        apply_config_to(request, *config_);
        return client_->do_request(std::move(request));
    }

    asyncnet::NetworkTask<asyncnet::Response> RequestorContext::request(PostRequest request) {
        apply_config_to(request, *config_);
        return client_->do_request(std::move(request));
    }

    asyncnet::NetworkTask<asyncnet::Response> RequestorContext::request(PostMultipartRequest request) {
        apply_config_to(request, *config_);
        return client_->do_request(std::move(request));
    }

    asyncnet::NetworkTask<asyncnet::Response> RequestorContext::request(std::shared_ptr<PolymorphicRequest> request) {
        throw std::runtime_error("Unsupported");
    }

    //void RequestorContext::info(std::string_view message, const std::source_location loc) {
    //    if (logger_) {
    //        logger_->log(LogLevel::Info, message, loc);
    //    }
    //}

    //void RequestorContext::debug(std::string_view message, const std::source_location loc) {
    //    if (logger_) {
    //        logger_->log(LogLevel::Debug, message, loc);
    //    }
    //}

    //void RequestorContext::warning(std::string_view message, const std::source_location loc) {
    //    if (logger_) {
    //        logger_->log(LogLevel::Warning, message, loc);
    //    }
    //}

    //void RequestorContext::error(std::string_view message, const std::source_location loc) {
    //    if (logger_) {
    //        logger_->log(LogLevel::Error, message, loc);
    //    }
    //}

    //void RequestorContext::fatal(std::string_view message, const std::source_location loc) {
    //    if (logger_) {
    //        logger_->log(LogLevel::Fatal, message, loc);
    //    }
    //}

    std::shared_ptr<const ClientConfig> RequestorContext::config() const {
        return config_;
    }

    size_t RequestorContext::alt_link() const {
        return alt_link_;
    }

    void RequestorContext::set_alt_link(size_t alt_link) {
        alt_link_ = alt_link;
    }

    RequestorContext RequestorContext::new_with_logger(std::shared_ptr<LoggerContext> logger) const {
        return RequestorContext(client_, logger, config_);
    }

    RequestorContext RequestorContext::new_with_client(std::shared_ptr<ClientContext> client) const {
        return RequestorContext(client, logger_, config_);
    }

    RequestorContext RequestorContext::new_with_config(std::shared_ptr<ClientConfig> config) const {
        return RequestorContext(client_, logger_, config);
    }

};
