/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/catalog/Search.hpp"

#include "aniparse/media/MediaInfo.hpp"

#include <algorithm>
#include <type_traits>
#include <set>
#include <cmath>

namespace aniparse {
namespace {

/**
 * Every identity axis the core names: which ids can be fed back into a search, and
 * which key takes them. Using the core spelling for a common axis is what lets a
 * consumer route an id collected off one source into a query against another — two
 * sources spelling one vocabulary differently are unjoinable.
 *
 * Adding a lookup means adding the key constant in @ref search_keys AND pairing it
 * here. Namespaces absent from the table (anidb, mangaupdates at the time of writing)
 * are ones no source offers a lookup for; they are emitted and stored all the same.
 */
struct IdentityAxis {
	std::string_view ns;
	std::string_view key;
};

constexpr IdentityAxis identity_axes[] = {
	{ id_namespaces::mal,       search_keys::mal_id       },
	{ id_namespaces::anilist,   search_keys::anilist_id   },
	{ id_namespaces::kitsu,     search_keys::kitsu_id     },
	{ id_namespaces::shikimori, search_keys::shikimori_id },
};

template <typename Capability> struct ValueFor;
template <> struct ValueFor<TextCapabilities> { using type = TextQuery; };
template <typename T> struct ValueFor<IntervalCapabilities<T>> { using type = Interval<T>; };
template <> struct ValueFor<CheckmarkCapabilities> { using type = Checkmark; };
template <> struct ValueFor<SelectionCapabilities> { using type = SelectionValue; };

bool exclusion_denied(const TextCapabilities& descriptor, const TextQuery& value) {
    return value.exclusive && !descriptor.supports_exclusion;
}

template<typename T>
bool exclusion_denied(const IntervalCapabilities<T>& descriptor, const Interval<T>& value) {
    return value.exclusive && !descriptor.supports_exclusion;
}

bool exclusion_denied(const CheckmarkCapabilities& descriptor, const Checkmark& value) {
    return value.exclusive && !descriptor.supports_exclusion;
}

bool exclusion_denied(const SelectionCapabilities& descriptor, const SelectionValue& value) {
    return std::any_of(value.excluded.begin(), value.excluded.end(), [&](const auto& token) {
        const auto found = descriptor.options.find(token);
        const bool supported = found != descriptor.options.end()
            ? found->second.supports_exclusion.value_or(descriptor.supports_exclusion)
            : descriptor.supports_exclusion;
        return !supported;
    });
}

bool value_invalid(const TextCapabilities&, const TextQuery& value) {
    return value.text.empty();
}

template<typename T>
bool value_invalid(const IntervalCapabilities<T>& descriptor, const Interval<T>& value) {
    if constexpr (std::is_floating_point_v<T>) {
        if ((value.from && !std::isfinite(*value.from)) ||
            (value.to && !std::isfinite(*value.to))) return true;
    }
    if (value.from && value.to && *value.to < *value.from) return true;
    auto outside = [&](const auto& bound) {
        return bound && ((descriptor.min && *bound < *descriptor.min)
            || (descriptor.max && *bound > *descriptor.max));
    };
    return outside(value.from) || outside(value.to);
}

bool value_invalid(const CheckmarkCapabilities&, const Checkmark&) { return false; }

bool value_invalid(const SelectionCapabilities& descriptor, const SelectionValue& value) {
    std::set<std::string_view> seen;
    for (const auto* tokens : {&value.included, &value.excluded}) {
        for (const auto& token : *tokens) {
            if (token.empty() || !seen.insert(token).second
                || (!descriptor.open_vocabulary && !descriptor.options.contains(token))) {
                return true;
            }
        }
    }
    return false;
}

std::string_view reason_text(SearchQueryError::Reason reason) noexcept {
	switch (reason) {
	case SearchQueryError::Reason::UnknownKey:
		return "unknown filter";
	case SearchQueryError::Reason::TypeMismatch:
		return "wrong value type for filter";
	case SearchQueryError::Reason::ExclusionNotSupported:
		return "exclusion is not supported for filter";
	case SearchQueryError::Reason::MultipleSelectionNotSupported:
		return "multiple selections are not supported for filter";
	case SearchQueryError::Reason::InvalidValue:
		return "invalid value for filter";
	case SearchQueryError::Reason::UnknownSortKey:
		return "unknown sort";
	case SearchQueryError::Reason::SortDirectionNotSupported:
		return "unsupported direction for sort";
	}
	return "invalid filter";
}

} // namespace

std::vector<SearchQueryError> validate_search_query(
	const SupportedSearchFilters& supported,
	const SearchRequestQuery& query) {
	std::vector<SearchQueryError> errors;
	for (const auto& [key, value] : query.filters) {
		auto found = supported.find(key);
		if (found == supported.end()) {
			errors.push_back({ .reason = SearchQueryError::Reason::UnknownKey, .key = key });
			continue;
		}
		std::visit([&](const auto& descriptor) {
            using Capability = std::remove_cvref_t<decltype(descriptor)>;
            using Value = typename ValueFor<Capability>::type;
            const auto* typed = std::get_if<Value>(&value);
            if (!typed) {
                errors.push_back({.reason = SearchQueryError::Reason::TypeMismatch, .key = key});
                return;
            }
            if (exclusion_denied(descriptor, *typed)) {
                errors.push_back({.reason = SearchQueryError::Reason::ExclusionNotSupported, .key = key});
            }
            if (value_invalid(descriptor, *typed)) {
                errors.push_back({.reason = SearchQueryError::Reason::InvalidValue, .key = key});
            }
            if constexpr (std::is_same_v<Capability, SelectionCapabilities>) {
                if (!descriptor.multiple && typed->included.size() + typed->excluded.size() > 1) {
                    errors.push_back({.reason = SearchQueryError::Reason::MultipleSelectionNotSupported, .key = key});
                }
            }
        }, found->second.capabilities);
	}
	return errors;
}

std::vector<SearchQueryError> validate_sort(
	const SupportedSorts& supported,
	const std::optional<SortOrder>& sort) {
	if (!sort) {
		return {};
	}
	auto found = supported.find(sort->key);
	if (found == supported.end()) {
		return { { .reason = SearchQueryError::Reason::UnknownSortKey, .key = sort->key } };
	}
	bool direction_supported = sort->ascending ? found->second.ascending : found->second.descending;
	if (!direction_supported) {
		return { { .reason = SearchQueryError::Reason::SortDirectionNotSupported, .key = sort->key } };
	}
	return {};
}

std::vector<SearchQueryError> validate_query(
	const SearchCapabilities& support,
	const SearchRequestQuery& query,
	const GetFilters& filters) {
	std::vector<SearchQueryError> errors = validate_search_query(support.supported_filters, query);
	std::vector<SearchQueryError> sort_errors = validate_sort(support.supported_sorts, filters.sort);
	errors.insert(errors.end(),
		std::make_move_iterator(sort_errors.begin()),
		std::make_move_iterator(sort_errors.end()));
	return errors;
}

std::string_view search_key_for(std::string_view ns) {
	auto found = std::find_if(std::begin(identity_axes), std::end(identity_axes),
		[&](const IdentityAxis& axis) { return axis.ns == ns; });
	return found != std::end(identity_axes) ? found->key : std::string_view{};
}

// The support table is unused for now: only the core names identity keys today, so
// the answer is the same for every source. The parameter is not dead weight — filter
// keys are an open vocabulary, so a parser may introduce an identity axis the core
// does not name, and the day one does, it declares that in its SearchCapabilities
// and this reads it there. Taking the table now is what keeps that change invisible
// to every caller.
bool is_identity_key(const SearchCapabilities& /*support*/, std::string_view key) {
	return std::any_of(std::begin(identity_axes), std::end(identity_axes),
		[&](const IdentityAxis& axis) { return axis.key == key; });
}

std::string describe_search_query_errors(std::span<const SearchQueryError> errors) {
	std::string message;
	for (const auto& error : errors) {
		if (!message.empty()) {
			message += "; ";
		}
		message += reason_text(error.reason);
		message += " '";
		message += error.key;
		message += "'";
	}
	return message;
}
} // namespace aniparse
