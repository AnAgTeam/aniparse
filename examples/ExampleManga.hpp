#pragma once
#include <aniparse/manga/MangaGetter.hpp>
#include <iterator>
#include <string_view>
#include <utility>

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
    RequestTask<PageResults<Recommendation<MangaGetter>>> load_similar(
        RequestContext, GetFilters filters) const override {
        if (filters.from < 0 || filters.sort) {
            co_return make_request_error(RequestErrorCode::InvalidArguments,
                                         "Expected a nonnegative offset and source ordering");
        }
        PageResults<Recommendation<MangaGetter>> page{.next_offset = filters.from};
        if (id_ != "first") co_return page;
        // Source-provided explanations belong to the pair, not to either work's Info.
        const std::pair<std::string_view, std::string_view> entries[] = {
            {"second", "Similar themes"}, {"third", ""},
        };
        page.total_count = std::size(entries);
        for (auto i = static_cast<std::size_t>(filters.from);
             i < std::size(entries) && page.results.size() < filters.limit; ++i) {
            page.append(filters.from, {
                .getter = std::make_unique<ExampleManga>(std::string(entries[i].first)),
                .reason = std::string(entries[i].second),
            });
        }
        co_return page;
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
