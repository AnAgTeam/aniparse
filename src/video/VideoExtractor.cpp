/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/video/VideoExtractor.hpp"

#include <stdexcept>

namespace aniparse {

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

void VideoExtractorStore::Edit::commit() {
	edit_.commit();
}

std::shared_ptr<VideoExtractor> VideoExtractorStore::add_extractor(std::shared_ptr<VideoExtractor> extractor) {
	auto edit = begin_edit();
	auto added = edit.add_extractor(std::move(extractor));
	edit.commit();
	return added;
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

} // namespace aniparse
