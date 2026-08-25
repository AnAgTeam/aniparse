/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/types/Search.hpp"

#include "aniparse/types/Model.hpp"

#include <algorithm>
#include <type_traits>

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

bool multiple_selection_denied(const ItemSelection& descriptor, const ItemSelection& value) {
	return descriptor.single_selection && value.size() > 1;
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
			if constexpr (std::is_same_v<std::remove_cvref_t<decltype(descriptor)>, ItemSelection>) {
				if (multiple_selection_denied(descriptor, typed)) {
					errors.push_back({ .reason = SearchQueryError::Reason::MultipleSelectionNotSupported, .key = key });
				}
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

std::string_view search_key_for(std::string_view ns) {
	auto found = std::find_if(std::begin(identity_axes), std::end(identity_axes),
		[&](const IdentityAxis& axis) { return axis.ns == ns; });
	return found != std::end(identity_axes) ? found->key : std::string_view{};
}

// The support table is unused for now: only the core names identity keys today, so
// the answer is the same for every source. The parameter is not dead weight — filter
// keys are an open vocabulary, so a parser may introduce an identity axis the core
// does not name, and the day one does, it declares that in its SearchCompatibilities
// and this reads it there. Taking the table now is what keeps that change invisible
// to every caller.
bool is_identity_key(const SearchCompatibilities& /*support*/, std::string_view key) {
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
