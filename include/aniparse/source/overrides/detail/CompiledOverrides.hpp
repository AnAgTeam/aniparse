#pragma once
#include "aniparse/cache/ResourceCache.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>

namespace aniparse::detail {

// Lazy compiled sets for one immutable source generation, isolated by owner/type.
// Compilation runs outside the lock; a concurrent losing build is discarded.
template<class Source>
class CompiledOverrides {
public:
    explicit CompiledOverrides(std::shared_ptr<const Source> source)
        : source_(source ? std::move(source) : std::make_shared<const Source>()) {}
    std::shared_ptr<const Source> source() const { return source_; }

    template<class T>
    std::shared_ptr<const T> get(std::string_view owner) const {
        const void* key = resource_type_key<T>();
        {
            std::shared_lock lock(mutex_);
            if (auto by_owner = sets_.find(owner); by_owner != sets_.end()) {
                if (auto it = by_owner->second.find(key); it != by_owner->second.end()) {
                    return std::static_pointer_cast<const T>(it->second);
                }
            }
        }
        auto source = view(owner);
        auto built = std::make_shared<const T>(T::make(*source));
        std::unique_lock lock(mutex_);
        auto [it, inserted] = sets_[std::string(owner)].try_emplace(key, std::move(built));
        return std::static_pointer_cast<const T>(it->second);
    }
private:
    std::shared_ptr<const Source> view(std::string_view owner) const {
        if (owner.empty() || !source_->has_scope(owner)) return source_;
        {
            std::shared_lock lock(mutex_);
            if (auto it = views_.find(owner); it != views_.end()) return it->second;
        }
        auto scoped = source_->scoped_to(owner);
        std::unique_lock lock(mutex_);
        auto [it, inserted] = views_.try_emplace(std::string(owner), std::move(scoped));
        return it->second;
    }
    const std::shared_ptr<const Source> source_;
    mutable std::shared_mutex mutex_;
    mutable std::map<std::string, std::shared_ptr<const Source>, std::less<>> views_;
    mutable std::map<std::string, std::map<const void*, std::shared_ptr<const void>>, std::less<>> sets_;
};

} // namespace aniparse::detail
