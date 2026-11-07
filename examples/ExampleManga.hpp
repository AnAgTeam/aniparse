#pragma once
#include <aniparse/manga/MangaGetter.hpp>

namespace aniparse::examples {

// Shared offline fixture, not a base class required by the library.
class ExampleManga final : public MangaGetter {
public:
    explicit ExampleManga(std::string id = "first") : id_(std::move(id)) {}
    MangaGetterCapabilities capabilities() const noexcept override { return {}; }
    std::optional<MangaInfo> preview_info() const noexcept override {
        return MangaInfo{.id = id_, .common = {.title = "Example " + id_}};
    }
    RequestTask<MangaInfo> load_info(RequestContext) const override {
        co_return *preview_info();
    }
    RequestTask<PageResults<MangaPage>> load_chapter_pages(
        RequestContext, MangaChapterRef, GetFilters,
        std::optional<MangaTranslationID>) const override {
        // A metadata-only source need not provide a reader.
        co_return make_request_error(RequestErrorCode::NotImplemented, "No reader");
    }
    std::string canonical_url(RequestContext) const override {
        return "https://example.com/manga/" + id_;
    }
    RequestTask<SerializedGetterData> serialize() const override {
        // This example's durable format is a stable slug, not a fetched Info.
        co_return SerializedGetterData{.url = id_};
    }
private:
    std::string id_;
};

} // namespace aniparse::examples
