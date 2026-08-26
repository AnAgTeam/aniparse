/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/video/VideoExtractor.hpp"
#include "aniparse/media/ResourceAdapter.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace aniparse {
namespace {

/**
 * Extract one already-routed URL; when the extraction delegates (links instead
 * of streams), route each link through the store and recurse, returning the
 * first extraction that yields direct streams. Links are alternatives: an
 * unroutable or failing link falls through to the next one.
 */
NetworkRequestTask<VideoExtraction> extract_routed(
    RequestorContext context, VideoExtractionRoute route,
    const VideoExtractorStore& store, int depth, int max_depth) {
	auto extraction = co_await route.extractor->extract(context, std::move(route.url));
	if (!extraction) {
		co_return unexpected(std::move(extraction.error()));
	}
	if (!extraction->streams.empty() || extraction->links.empty()) {
		co_return std::move(*extraction);
	}
	if (depth >= max_depth) {
		// Chain cut: hand the unresolved extraction back as-is, so the consumer
		// still sees the delegated links that were not followed.
		co_return std::move(*extraction);
	}
	for (const VideoExtractionLink& link : extraction->links) {
		auto next = store.route_link(link);
		if (!next) {
			continue;
		}
		auto followed = co_await extract_routed(context, std::move(*next), store, depth + 1, max_depth);
		if (followed && !followed->streams.empty()) {
			// A delegated stream passes through the current extractor before it
			// reaches the delegated provider. Preserve that nesting for both the
			// transport defaults and any byte/resource transformations.
			followed->headers.merge_missing(extraction->headers);
			followed->adapter = ResourceAdapter::compose({ extraction->adapter, followed->adapter });
			co_return std::move(*followed);
		}
	}
	co_return std::move(*extraction);
}

} // namespace

Mirrors VideoExtractor::mirrors(const RequestorContext& context,
                                std::span<const std::string_view> builtin) const {
	return context.mirrors(identifier(), builtin);
}

std::string_view VideoExtractor::base_url(const RequestorContext& context,
                                          std::span<const std::string_view> builtin) const {
	// First mirror: extractors have no per-user mirror selection.
	return mirrors(context, builtin).base_url(0);
}

std::shared_ptr<VideoExtractor> VideoExtractorStore::Edit::add_extractor(
    std::shared_ptr<VideoExtractor> extractor) {
	if (!extractor) {
		throw std::invalid_argument("VideoExtractor must not be null");
	}
	std::string identifier = extractor->identifier();
	if (identifier.empty()) {
		throw std::logic_error("VideoExtractor identifier must not be empty");
	}
	if (edit_.contains(identifier)) {
		throw std::logic_error("Conflicting video extractor name: " + identifier);
	}
	return edit_.add(std::move(identifier), std::move(extractor));
}

bool VideoExtractorStore::Edit::remove_extractor(std::string_view identifier) {
	return edit_.remove(identifier);
}

void VideoExtractorStore::Edit::refresh_domains(
    std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains) {
	edit_.set_volatile_domains(std::move(volatile_domains));
}

void VideoExtractorStore::Edit::commit() {
	edit_.commit();
}

std::shared_ptr<VideoExtractor> VideoExtractorStore::add_extractor(std::shared_ptr<VideoExtractor> extractor) {
	auto edit = begin_edit();
	auto added = edit.add_extractor(std::move(extractor));
	edit.commit();
	return added;
}

void VideoExtractorStore::refresh_domains(
    std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains) {
	auto edit = begin_edit();
	edit.refresh_domains(std::move(volatile_domains));
	edit.commit();
}

std::shared_ptr<VideoExtractor> VideoExtractorStore::find_by_key(std::string_view identifier) const {
	return domains_.find_by_key(identifier);
}

std::vector<std::shared_ptr<VideoExtractor>> VideoExtractorStore::extractors() const {
	return domains_.values();
}

std::optional<VideoExtractionRoute> VideoExtractorStore::route_url(std::string_view url) const {
	auto parsed = ParsedUrl::parse(url);
	if (!parsed) {
		return std::nullopt;
	}
	return route_url(std::move(*parsed));
}

std::optional<VideoExtractionRoute> VideoExtractorStore::route_url(ParsedUrl url) const {
	auto extractor = domains_.find_by_host(url.host(), [&url](const VideoExtractor& candidate) {
		return candidate.valid_for_url(url);
	});
	if (!extractor) {
		return std::nullopt;
	}
	return VideoExtractionRoute{ std::move(extractor), std::move(url) };
}

std::optional<VideoExtractionRoute> VideoExtractorStore::route_link(const VideoExtractionLink& link) const {
	auto parsed = ParsedUrl::parse(link.url);
	if (!parsed) {
		return std::nullopt;
	}
	if (!link.extractor_id) {
		return route_url(std::move(*parsed));
	}
	auto extractor = find_by_key(*link.extractor_id);
	if (!extractor || !extractor->valid_for_url(*parsed)) {
		return std::nullopt;
	}
	return VideoExtractionRoute{ std::move(extractor), std::move(*parsed) };
}

NetworkRequestTask<VideoExtraction> VideoExtractorStore::extract(
    RequestorContext context, ParsedUrl url, int max_depth) const {
	const std::string href(url.href());
	auto route = route_url(std::move(url));
	if (!route) {
		co_return make_response_error(RequestErrorCode::NotImplemented,
		    "No video extractor handles " + href);
	}
	auto extraction = co_await extract_routed(std::move(context), std::move(*route), *this, 0, max_depth);
	co_return std::move(extraction);
}

NetworkRequestTask<VideoExtraction> VideoExtractorStore::extract(
    RequestorContext context, std::string url, int max_depth) const {
	auto parsed = ParsedUrl::parse(url);
	if (!parsed) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		    "Malformed video URL: " + url);
	}
	auto extraction = co_await extract(std::move(context), std::move(*parsed), max_depth);
	co_return std::move(extraction);
}

} // namespace aniparse
