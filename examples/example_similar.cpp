// Read source explanations alongside independently owned recommendation getters.
#include "ExampleClient.hpp"
#include "ExampleManga.hpp"
#include <aniparse/source/ParserProfile.hpp>
#include <coro/sync_wait.hpp>
#include <print>

using namespace aniparse;

int main() {
    auto profile = examples::make_profile();
    auto context = profile->make_session(examples::make_services()).snapshot();
    examples::ExampleManga source;
    constexpr std::size_t page_size = 1; // Small windows demonstrate continuation.
    pageoff from = 0;
    for (;;) {
        // Keep the receiver alive until the task completes.
        auto page = coro::sync_wait(source.load_similar(context, {.from = from, .limit = page_size}));
        if (!page) return 1;
        if (page->results.empty()) break;
        for (auto& entry : page->results) {
            auto& recommendation = entry.item;
            auto info = coro::sync_wait(recommendation.getter->load_info(context));
            if (!info) return 1;
            std::println("{}: {}", info->common.title,
                         recommendation.reason.empty() ? "No explanation supplied" : recommendation.reason);
            // Serialization preserves the destination identity, not this pair's reason.
            auto saved = coro::sync_wait(recommendation.getter->serialize());
            if (!saved) return 1;
        }
        from = page->next_offset;
    }
}
