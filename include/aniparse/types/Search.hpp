/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/catalog/SearchQuery.hpp"
#include "aniparse/catalog/SearchCapabilities.hpp"
#include "aniparse/types/Pagination.hpp"
#include "aniparse/types/Flags.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/**
 * @file
 * The search vocabulary shared by every domain: the query and filter types a
 * getter accepts, the support table it advertises (SearchCapabilities), and
 * the free functions that check one against the other — validate_query and its
 * parts — so a caller can pre-flight a query synchronously, before spending a
 * request on a filter the source would reject.
 */

namespace aniparse {
/**
 * @brief One violation found by validate_search_query
 * @see validate_search_query
 */
struct SearchQueryError {
	enum class Reason {
		/// The filter key is not present in the supported set
		UnknownKey,
		/// The filter holds a different SearchFilterValue alternative than declared
		TypeMismatch,
		/// The filter requests exclusion, but the descriptor does not allow it
		ExclusionNotSupported,
		/// The filter selects multiple tokens, but the descriptor accepts only one
		MultipleSelectionNotSupported,
		/// The value itself is malformed: empty text, inverted or out-of-bounds
		/// interval, selection of an undeclared item
		InvalidValue,
		/// The sort key is not present in the supported set
		UnknownSortKey,
		/// The sort key is supported, but not in the requested direction
		SortDirectionNotSupported,
	};

	Reason reason;
	/// Key of the offending entry in SearchRequestQuery::filters
	std::string key;

	friend bool operator==(const SearchQueryError&, const SearchQueryError&) = default;
};

/**
 * @brief Validate query filters against a declared support table.
 * Keeps supported_filters the single source of truth: parsers call it at the
 * start of search(), UI can call it pre-flight to highlight invalid inputs.
 * @param supported Declared filters (SearchCapabilities::supported_filters)
 * @param query The query to check
 * @return Empty if the query is valid; otherwise all violations found
 */
[[nodiscard]] std::vector<SearchQueryError> validate_search_query(
    const SupportedSearchFilters& supported,
    const SearchRequestQuery& query);

/**
 * @brief Validate a requested sort order against an endpoint's declaration.
 * Same contract as validate_search_query, for the GetFilters::sort channel:
 * no sort requested is always valid (the source's default order).
 * @param supported Declared sorts of the endpoint being called
 * @param sort The requested order (GetFilters::sort)
 * @return Empty if the sort is valid; otherwise the violation
 */
[[nodiscard]] std::vector<SearchQueryError> validate_sort(
    const SupportedSorts& supported,
    const std::optional<SortOrder>& sort);

/**
 * @brief Human-readable one-line summary of validation errors.
 * For the RequestError::message channel; UI should use the typed
 * errors from validate_search_query instead.
 */
[[nodiscard]] std::string describe_search_query_errors(std::span<const SearchQueryError> errors);

/**
 * @brief One autocomplete suggestion for a search token (a tag, a title, ...).
 * @c value is the token to insert into the query — the same search-token identity
 * as Tag::ref, so a chosen suggestion drops straight back into search(). @c category
 * names which search axis the token belongs to, from the same search-key vocabulary
 * as the suggest() @c kind hint and supported_filters. @see search_keys, suggest
 */
struct SearchSuggestion {
	/// The token to insert into the query (equals a Tag::ref search token).
	std::string value;
	/// Display label for the UI (e.g. "cirno"); may differ from @c value.
	std::string label;
	/// Popularity of the token when the source reports it (e.g. post count).
	std::optional<long> count;
	/// Which search axis this token is (a search key); empty when untyped.
	std::optional<std::string> category;
};

/**
 * @brief Validate a query + sort against an already-fetched support table.
 * Combines validate_search_query (filters) and validate_sort (sort) in one
 * pass: await the support once, then validate here — at the start of search()
 * or as a UI pre-flight.
 * @return Empty if valid; otherwise all violations found.
 */
[[nodiscard]] std::vector<SearchQueryError> validate_query(
    const SearchCapabilities& support,
    const SearchRequestQuery& query,
    const GetFilters& filters);
} // namespace aniparse

/**
 * Namespace for default keys, that can be used for search
 * @see SearchRequestQuery
 */
namespace aniparse::search_keys {
/// Filter by Series. Usually TextQuery. @see Series
inline constexpr std::string_view series          = "series";
/// Filter by page count. Usually IntInterval.
inline constexpr std::string_view pages           = "page_count";
/// Filter by episode count. Usually IntInterval.
inline constexpr std::string_view episodes        = "episode_count";
/// Filter by broadcast season. Usually SelectionValue.
inline constexpr std::string_view season          = "season";
/// Filter by the duration of one episode in minutes. Usually IntInterval.
inline constexpr std::string_view episode_duration = "episode_duration";
/// Filter by voice-over or subtitle track. Usually SelectionValue or TextQuery.
inline constexpr std::string_view voice           = "voice";
/// Filter by Tag. TextQuery for free-text sources; SelectionValue where the
/// source enumerates its tags, keyed by the opaque token that equals Tag::ref
/// so a tag from MangaInfo searches directly. @see SelectionValue
/// The default @ref aniparse::Tag::axis — a Tag with an empty axis filters through this key.
inline constexpr std::string_view tag             = "tag";
/// Filter by genre — the coarse editorial category axis a source keeps SEPARATE
/// from its finer tag axis (a source with one flat tag list uses @ref tag alone
/// and never this). Usually SelectionValue, keyed like @ref tag. @see SelectionValue
inline constexpr std::string_view genre           = "genre";
/// Filter by upload time (when the item was last updated). Usually TimeInterval
/// or RelativeTimeInterval.
inline constexpr std::string_view upload_time     = "upd_time";
/// Filter by release time (when the item was actually released). Usually
/// TimeInterval or RelativeTimeInterval.
inline constexpr std::string_view release_time    = "rel_time";
/// Filter by title (substring match, not necessarily full). Usually TextQuery.
inline constexpr std::string_view title           = "title";
/// Filter by status (aired state, e.g. "Announced"/"Released"). Usually
/// SelectionValue, or TextQuery.
inline constexpr std::string_view status          = "status";
/// Filter by Rating. Usually IntInterval (a score range) or SelectionValue.
inline constexpr std::string_view rating          = "rating";
/// Filter by year. Usually IntInterval.
inline constexpr std::string_view year            = "year";
/// Filter by AgeRestriction. Usually IntInterval or SelectionValue.
inline constexpr std::string_view age_restriction = "age_res";

// Categorical tag axes. On sources that expose them as enumerable filters (e.g.
// a booru/doujin catalog: character:, artist:, group:, ...) these are both filter
// keys in supported_filters AND the token kinds suggest() completes — one vocabulary
// for search and autocomplete. A source uses whichever apply. @see suggest
//
// These keys — together with @ref tag and @ref genre above — are the vocabulary of
// @ref aniparse::Tag::axis. A Tag carries the key of the axis it belongs to; the pair
// (Tag::axis, Tag::ref) then names one option in the supported_filters group keyed by
// that axis, so a tag read off an item routes straight back into the right filter and
// a consumer may section its tag display by axis. An empty Tag::axis means @ref tag.
/// Filter/suggest by artist or author tag. Usually SelectionValue or TextQuery.
inline constexpr std::string_view artist          = "artist";
/// Filter/suggest by character tag.
inline constexpr std::string_view character       = "character";
/// Filter/suggest by circle/group (doujin publisher).
inline constexpr std::string_view group           = "group";
/// Filter/suggest by work type (manga/doujinshi/artist CG/...).
inline constexpr std::string_view type            = "type";
/// Filter/suggest by language.
inline constexpr std::string_view language        = "language";

// Identity axes — one key per id vocabulary (@see id_namespaces). The inverse of
// ExternalId: that field is what a source *emits* about an item, these keys are
// what a source will *accept back* to find one. Each vocabulary gets its own key
// rather than one generic "external id" key, so that the declared key set states
// exactly which lookups a parser offers and the machinery that already rejects an
// unsupported key checks it.
//
// These are lookups, not filters: a token names one item exactly, so there is no
// substring match to run and no sensible way to invert one. A consumer that shows
// filters (a UI panel, `support search`) keeps them off that surface and drives
// them from wherever ids arrive instead — an id paste box, a deep link, a library
// import. @see is_identity_key
//
// **The value is a SelectionValue, not a TextQuery** —
// though a single id looks like free text, it is a token identity, and selection
// semantics are what carry the batch: a selection holds many tokens, and a source
// whose API accepts a set of ids in one call resolves the whole selection in ONE
// request. That is what makes opening fifty saved ids cheap, and it is the reason
// not to "simplify" these keys to a single-string type. Validation of an open
// vocabulary accepts any token when SelectionCapabilities::open_vocabulary is set. A token IS the id, so an
// ExternalId collected off one source drops straight into a query against another.
//
// **A parser that owns a vocabulary must accept its own ids.** Others emit them
// (an ExternalId naming this site), and a consumer that holds one has nothing else
// to open the item with: it cannot forge the parser's SerializedGetterData, and the
// id often arrives with no getter attached — from a precomputed table, or off a
// third source. Without the key, every id pointing at this parser is a dead end.
// Accepting a *foreign* vocabulary, by contrast, is a rare bonus.

/// Look up by MyAnimeList id — the cross-site hub most metadata sources carry.
/// @see id_namespaces::mal
inline constexpr std::string_view mal_id          = "mal_id";
/// Look up by AniList id. @see id_namespaces::anilist
inline constexpr std::string_view anilist_id      = "anilist_id";
/// Look up by Kitsu id. @see id_namespaces::kitsu
inline constexpr std::string_view kitsu_id        = "kitsu_id";
/// Look up by Shikimori id. @see id_namespaces::shikimori
inline constexpr std::string_view shikimori_id    = "shikimori_id";
} // namespace aniparse::search_keys

namespace aniparse {
/**
 * @brief The search key that looks ids of an @ref aniparse::id_namespaces vocabulary
 *        up; empty when the core names no lookup for it.
 *
 * The bridge an id crosses to become a query: an ExternalId collected off one source
 * carries a namespace, and this maps it to the key another source accepts it under —
 * so a consumer routes ids without hardcoding the pairing.
 *
 * The two spellings stay distinct on purpose ("mal" vs "mal_id"). Both vocabularies
 * are open — a parser may emit a namespace the core does not name, and may accept a
 * key the core does not name — so nothing structurally stops the two spaces from
 * overlapping; the @c _id suffix keeps them apart by convention, which is what makes
 * a namespace no source looks up by fail loudly (SearchQueryError::Reason::UnknownKey)
 * rather than silently match an unrelated filter of the same name. The mapping is
 * partial for the same reason: a namespace with no key is one no source offers a
 * lookup for — it can still be emitted and stored, just not searched by.
 */
[[nodiscard]] std::string_view search_key_for(std::string_view ns);

/**
 * @brief Whether a filter key is an exact id lookup rather than a user-facing filter.
 *
 * Scoped to one source's support table because what counts as a lookup axis is a
 * property of the source, even while the core names the common keys — a caller asks
 * "is this key, on this source, a lookup?". Every caller already holds the table: it
 * is what they are iterating to render or validate.
 *
 * A consumer that presents filters uses this to keep lookups out of the filter
 * surface: they take an id nobody types by hand, they cannot be excluded ('!'), and
 * combining one with a filter is pointless even where the source technically allows
 * it. Ids reach a query from where ids actually come from — a paste box, a deep link,
 * a library import — via search_key_for. @see search_keys, search_key_for
 */
[[nodiscard]] bool is_identity_key(const SearchCapabilities& support, std::string_view key);
} // namespace aniparse
