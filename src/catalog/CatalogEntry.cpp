/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/catalog/CatalogEntry.hpp"

namespace aniparse {

bool CatalogEntry::has(MediaKind kind) const noexcept {
    switch (kind) {
    case MediaKind::Manga: return manga != nullptr;
    case MediaKind::Anime: return anime != nullptr;
    }
    return false;
}

std::optional<MediaInfo> CatalogEntry::preview_info(std::optional<MediaKind> preferred) const {
	auto read = [this](MediaKind kind) -> std::optional<MediaInfo> {
		if (kind == MediaKind::Manga && manga) {
			if (auto info = manga->preview_info()) return std::move(info->common);
		}
		if (kind == MediaKind::Anime && anime) {
			if (auto info = anime->preview_info()) return std::move(info->common);
		}
		return std::nullopt;
	};
	const auto first = preferred.value_or(preferred_preview.value_or(MediaKind::Manga));
	if (auto preview = read(first)) return preview;
	if (first != MediaKind::Manga) {
		if (auto preview = read(MediaKind::Manga)) return preview;
	}
	if (first != MediaKind::Anime) return read(MediaKind::Anime);
	return std::nullopt;
}


} // namespace aniparse
