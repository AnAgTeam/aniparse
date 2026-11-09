#pragma once

#include <boost/json.hpp>
#include <concepts>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace aniparse::json {

/// Compiled structural path: .field, [index], ["quoted.key"], and array [*].
/// A lone dot selects the current node. Invalid syntax throws invalid_argument.
/// Conditions and conversions belong to C++ ranges, not the path language.
class Selector {
public:
    struct Wildcard {};
    using Step = std::variant<std::string, std::size_t, Wildcard>;
    explicit Selector(std::string_view path);
    const std::vector<Step>& steps() const { return steps_; }
private:
    std::vector<Step> steps_;
};

/// Scalar reads own their results; borrowed strings and JSON references are excluded.
template<class T>
concept Scalar = std::same_as<T, std::string> || std::same_as<T, bool>
    || std::integral<T> || std::floating_point<T>;

/// Read-only JSON node: lvalues are borrowed, rvalues are owned by shared handles.
/// Borrowed documents and their storage must outlive every derived node and range;
/// do not mutate or move the document while those handles are in use. Owning nodes
/// retain moved documents. Rvalues with non-owning custom storage are copied into
/// default storage so a caller's memory resource cannot dangle.
/// A default View is missing; an explicit JSON null is present but has no scalar value.
/// Potentially deprecated: model conversion via json/Conversion.hpp is being
/// evaluated as the primary parser API. View remains supported during that trial;
/// no compiler deprecation attribute is applied yet.
class View {
public:
    View() = default;
    explicit View(const boost::json::value& document) : node_(&document) {}
    explicit View(const boost::json::object& document) : node_(&document) {}
    explicit View(const boost::json::array& document) : node_(&document) {}
    explicit View(boost::json::value&& document);
    explicit View(boost::json::object&& document) : View(boost::json::value(std::move(document))) {}
    explicit View(boost::json::array&& document) : View(boost::json::value(std::move(document))) {}
    View(const boost::json::value&&) = delete;
    View(const boost::json::object&&) = delete;
    View(const boost::json::array&&) = delete;
    // Keep moved-from views valid and owning, just like copies.
    View(const View&) = default;
    View& operator=(const View&) = default;

    bool exists() const { return std::visit([](auto pointer) { return pointer != nullptr; }, node_); }
    /// True for an object, including an empty object; false for missing/null nodes.
    bool is_object() const { return object() != nullptr; }
    /// True for an array, including an empty array; false for missing/null nodes.
    bool is_array() const { return array() != nullptr; }
    View field(std::string_view key) const;
    View index(std::size_t index) const;
    std::size_t array_size() const;

    /// Select one node. Wildcards are rejected, even when the input is missing.
    View pick(const Selector& selector) const;
    View pick(std::string_view path) const { return pick(Selector(path)); }

    /// Strict scalar conversion: no string coercion or fractional truncation.
    /// Integers must be stored as JSON integers and fit the destination type.
    /// Floating reads accept JSON numbers; precision rounding is permitted.
    template<Scalar T>
    std::optional<T> as() const {
        const auto* pointer = std::get_if<const boost::json::value*>(&node_);
        const auto* value = pointer ? *pointer : nullptr;
        if (!value) return std::nullopt;
        if constexpr (std::same_as<T, std::string>) {
            if (const auto* s = value->if_string()) return std::string(s->data(), s->size());
        } else if constexpr (std::same_as<T, bool>) {
            if (const auto* b = value->if_bool()) return *b;
        } else if constexpr (std::integral<T>) {
            if (const auto* n = value->if_int64(); n && std::in_range<T>(*n)) return static_cast<T>(*n);
            if (const auto* n = value->if_uint64(); n && std::in_range<T>(*n)) return static_cast<T>(*n);
        } else {
            long double number;
            if (const auto* n = value->if_double()) number = *n;
            else if (const auto* n = value->if_int64()) number = *n;
            else if (const auto* n = value->if_uint64()) number = *n;
            else return std::nullopt;
            if (number >= std::numeric_limits<T>::lowest() && number <= std::numeric_limits<T>::max())
                return static_cast<T>(number);
        }
        return std::nullopt;
    }

private:
    View(std::shared_ptr<const boost::json::value> owner, const boost::json::value* node)
        : owner_(std::move(owner)), node_(node) {}
    std::shared_ptr<const boost::json::value> owner_;
    using Node = std::variant<const boost::json::value*, const boost::json::object*, const boost::json::array*>;
    Node node_{static_cast<const boost::json::value*>(nullptr)};
    const boost::json::object* object() const;
    const boost::json::array* array() const;
};

/// Materialized selection of node handles, interoperable with C++20 ranges.
/// Each handle preserves its input ownership; borrowing inputs remain borrowed.
/// Copies share immutable handles. Iterators require this range (or a copy) alive.
class Selection : public std::ranges::view_interface<Selection> {
public:
    Selection() : nodes_(std::make_shared<const std::vector<View>>()) {}
    explicit Selection(std::vector<View> nodes)
        : nodes_(std::make_shared<const std::vector<View>>(std::move(nodes))) {}
    auto begin() const { return nodes_->begin(); }
    auto end() const { return nodes_->end(); }
private:
    std::shared_ptr<const std::vector<View>> nodes_;
};

/// Pipe adapter selecting relative to each input node and flattening in input order.
/// Missing paths and wrong container types contribute no nodes. JSON null is retained.
/// Selection is eager; subsequent standard range adapters may be lazy.
class Select {
public:
    explicit Select(Selector selector) : selector_(std::move(selector)) {}
    Selection operator()(View root) const;
    friend Selection operator|(View root, const Select& select) { return select(root); }

    template<std::ranges::input_range R>
        requires std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, View>
    friend Selection operator|(R&& roots, const Select& select) {
        std::vector<View> result;
        for (const auto& root : roots) {
            for (const auto& node : select(root)) result.push_back(node);
        }
        return Selection(std::move(result));
    }
private:
    Selector selector_;
};

namespace views {

inline Select select(std::string_view path) { return Select(Selector(path)); }
inline Select select(Selector selector) { return Select(std::move(selector)); }

/// Lazy scalar extraction, explicitly skipping missing and incompatible values.
/// String results own their bytes. Node ranges preserve the original document ownership;
/// borrowed JSON must remain alive until iteration is finished.
template<Scalar T>
auto values() {
    return std::views::transform([](View node) { return node.as<T>(); })
        | std::views::filter([](const std::optional<T>& value) { return value.has_value(); })
        | std::views::transform([](std::optional<T> value) { return *std::move(value); });
}

} // namespace views

} // namespace aniparse::json
