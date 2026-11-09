#pragma once
#include "aniparse/utility/AtomicSharedPtr.hpp"
#include "aniparse/source/overrides/detail/CompiledOverrides.hpp"
#include "aniparse/html/SelectorSource.hpp"
#include "aniparse/utility/RegexSource.hpp"
#include "aniparse/source/overrides/MirrorSource.hpp"

namespace aniparse {

/**
 * @brief One immutable generation of request overrides and its compiled caches.
 * Sources never change after publication. Lazy caches are private to this
 * generation, so a late compilation cannot populate a newer generation.
 */
class OverridesState {
public:
    /**
     * @brief Assemble an unpublished generation.
     * @param mirrors Mirror and frontend overrides, or null for built-ins.
     * @param selectors Selector overrides, or null for built-ins.
     * @param patterns Pattern overrides, or null for built-ins.
     * @param revision Verified payload revision; zero denotes built-ins.
     */
    explicit OverridesState(std::shared_ptr<const MirrorSource> mirrors = {},
                            std::shared_ptr<const html::SelectorSource> selectors = {},
                            std::shared_ptr<const RegexSource> patterns = {},
                            uint64_t revision = 0)
        : mirrors(mirrors ? std::move(mirrors) : std::make_shared<const MirrorSource>()), revision(revision),
          selectors_(std::move(selectors)), patterns_(std::move(patterns)) {}

    const std::shared_ptr<const MirrorSource> mirrors;
    const uint64_t revision;

    /**
     * @return This generation's selector source.
     */
    auto selector_source() const { return selectors_.source(); }
    /**
     * @return This generation's pattern source.
     */
    auto pattern_source() const { return patterns_.source(); }
    /**
     * @brief Resolve a compiled selector set in this generation.
     * @tparam T Compiled selector set type.
     * @param owner Parser or extractor identity.
     * @return Shared compiled set, retained independently of later publications.
     */
    template<class T> auto selectors(std::string_view owner) const {
        return selectors_.get<T>(owner);
    }
    /**
     * @brief Resolve a compiled pattern set in this generation.
     * @tparam T Compiled pattern set type.
     * @param owner Parser or extractor identity.
     * @return Shared compiled set, retained independently of later publications.
     */
    template<class T> auto patterns(std::string_view owner) const {
        return patterns_.get<T>(owner);
    }
private:
    detail::CompiledOverrides<html::SelectorSource> selectors_;
    detail::CompiledOverrides<RegexSource> patterns_;
};

/// Publishes complete generations; null means the host has not applied overrides.
using OverridesStore = AtomicSharedPtr<const OverridesState>;

} // namespace aniparse
