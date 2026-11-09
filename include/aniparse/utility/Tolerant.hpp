#pragma once

#include <memory>
#include <optional>
#include <utility>

namespace aniparse {

/**
 * @brief An optional value whose decoding failure may be discarded by a parser.
 * @tparam T The owned value type.
 * @note This type only stores the value; the document converter implements the
 *       error policy. JSON integration lives in aniparse/json/Conversion.hpp.
 *       Empty means absent, null, or rejected input; it does not retain an error.
 */
template<class T>
class Tolerant {
public:
    using value_type = T;

    Tolerant() = default;
    Tolerant(std::nullopt_t) noexcept {}
    explicit Tolerant(T value) : value_(std::move(value)) {}

    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    void reset() noexcept { value_.reset(); }

    T& value() & { return value_.value(); }
    const T& value() const& { return value_.value(); }
    T&& value() && { return std::move(value_).value(); }
    const T&& value() const&& { return std::move(value_).value(); }

    T& operator*() & noexcept { return *value_; }
    const T& operator*() const& noexcept { return *value_; }
    T&& operator*() && noexcept { return *std::move(value_); }
    const T&& operator*() const&& noexcept { return *std::move(value_); }
    T* operator->() noexcept { return std::addressof(*value_); }
    const T* operator->() const noexcept { return std::addressof(*value_); }

    template<class U>
    T value_or(U&& fallback) const& { return value_.value_or(std::forward<U>(fallback)); }
    template<class U>
    T value_or(U&& fallback) && { return std::move(value_).value_or(std::forward<U>(fallback)); }

private:
    std::optional<T> value_;
};

} // namespace aniparse
