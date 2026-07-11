/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/types/Search.hpp"

#include <algorithm>
#include <type_traits>

namespace aniparse {
namespace {

bool exclusion_denied(const TextQuery& descriptor, const TextQuery& value) {
	return value.exclusive && !descriptor.exclusive;
}

template<typename T>
bool exclusion_denied(const Interval<T>& descriptor, const Interval<T>& value) {
	return value.exclusive && !descriptor.exclusive;
}

bool exclusion_denied(const Checkmark& descriptor, const Checkmark& value) {
	return value.exclusive && !descriptor.exclusive;
}

bool exclusion_denied(const ItemSelection& descriptor, const ItemSelection& value) {
	return std::any_of(value.begin(), value.end(), [&](const auto& item) {
		auto found = descriptor.find(item.first);
		return found != descriptor.end() && item.second.exclusive && !found->second.exclusive;
	});
}

bool value_invalid(const TextQuery&, const TextQuery& value) {
	return value.text.empty();
}

// The descriptor's own bounds are the allowed min/max
template<typename T>
bool value_invalid(const Interval<T>& descriptor, const Interval<T>& value) {
	if (value.from && value.to && *value.to < *value.from) {
		return true;
	}
	if (descriptor.from && value.from && *value.from < *descriptor.from) {
		return true;
	}
	if (descriptor.to && value.to && *descriptor.to < *value.to) {
		return true;
	}
	return false;
}

bool value_invalid(const Checkmark&, const Checkmark&) {
	return false;
}

bool value_invalid(const ItemSelection& descriptor, const ItemSelection& value) {
	// An empty descriptor is an open-vocabulary axis: the source declares the axis but
	// does not enumerate its tokens (e.g. a source with millions of tags), so any
	// token is valid. A non-empty descriptor is a closed set — every token must be one
	// of the declared options.
	if (descriptor.empty()) {
		return false;
	}
	return std::any_of(value.begin(), value.end(), [&](const auto& item) {
		return descriptor.find(item.first) == descriptor.end();
	});
}

std::string_view reason_text(SearchQueryError::Reason reason) noexcept {
	switch (reason) {
	case SearchQueryError::Reason::UnknownKey:
		return "unknown filter";
	case SearchQueryError::Reason::TypeMismatch:
		return "wrong value type for filter";
	case SearchQueryError::Reason::ExclusionNotSupported:
		return "exclusion is not supported for filter";
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
	const SearchItems& supported,
	const SearchRequestQuery& query) {
	std::vector<SearchQueryError> errors;
	for (const auto& [key, value] : query.filters) {
		auto found = supported.find(key);
		if (found == supported.end()) {
			errors.push_back({ .reason = SearchQueryError::Reason::UnknownKey, .key = key });
			continue;
		}
		if (found->second.index() != value.index()) {
			errors.push_back({ .reason = SearchQueryError::Reason::TypeMismatch, .key = key });
			continue;
		}
		std::visit([&](const auto& descriptor) {
			const auto& typed = std::get<std::remove_cvref_t<decltype(descriptor)>>(value);
			if (exclusion_denied(descriptor, typed)) {
				errors.push_back({ .reason = SearchQueryError::Reason::ExclusionNotSupported, .key = key });
			}
			if (value_invalid(descriptor, typed)) {
				errors.push_back({ .reason = SearchQueryError::Reason::InvalidValue, .key = key });
			}
		}, found->second);
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
	const SearchCompatibilities& support,
	const SearchRequestQuery& query,
	const GetFilters& filters) {
	std::vector<SearchQueryError> errors = validate_search_query(support.supported_filters, query);
	std::vector<SearchQueryError> sort_errors = validate_sort(support.supported_sorts, filters.sort);
	errors.insert(errors.end(),
		std::make_move_iterator(sort_errors.begin()),
		std::make_move_iterator(sort_errors.end()));
	return errors;
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
