/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
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
 * getter accepts, the support table it advertises (SearchCompatibilities), and
 * the free functions that check one against the other — validate_query and its
 * parts — so a caller can pre-flight a query synchronously, before spending a
 * request on a filter the source would reject.
 */

namespace aniparse {
/**
 * @brief One text input
 */
struct TextQuery {
	/**
	 * Search context: text value of an item
	 * Support check: Name of input
	 */
	std::string text;
	/**
	 * Search context: Is text inversed (i.e. include not containing text)
	 * Support check: Is inverse supported
	 */
	bool exclusive = false;
};

enum class TimeIntervalPrecision {
	Any,
	Now,
	Hour,
	Day,
	Week,
	Month,
	Year,
};

/**
 * @brief Basic interval [from, to]
 */
template <typename T>
struct Interval {
	/**
	 * Search context: No limit or start of interval
	 * Support check: Min value of interval
	 */
	std::optional<T> from;
	/**
	 * Search context: No limit or end of interval
	 * Support check: Max value of interval
	 */
	std::optional<T> to;
	/**
	 * Search context: Is interval exclusive (not containing in interval).
	 *                 For example exclusive interval [10, 20] => (-inf;10]U[20;+inf)
	 * Support check: Is exclusive supported
	 */
	bool exclusive = false;
};

/**
 * @brief Integer interval
 * Can be used for pages count, episodes count, etc.
 */
using IntInterval = Interval<std::ptrdiff_t>;

/**
 * @brief Time interval
 * Can be used for update time, etc.
 */
using TimeInterval = Interval<std::chrono::system_clock::time_point>;

/**
 * @brief Relative time interval
 * Counts time from now(). Can be used for update time, etc.
 */
using RelativeTimeInterval = Interval<std::chrono::system_clock::duration>;

/**
 * @brief State of one selectable option: its display label and whether the
 *        selection is inverted. Presence in the ItemSelection marks the option
 *        as in play; the opaque token is the map key, not stored here.
 */
struct ItemSelectionValue {
	/**
	 * Human-readable label for this option — what the UI shows and the user
	 * picks by. Display only: not the machine token (that is the map key), and
	 * it need not be unique across options.
	 */
	std::string name;
	/**
	 * Whether the selection is inverted: in a query, exclude items matching this
	 * option instead of including them; in a support table, whether exclusion is
	 * offered for it. Honored only where the descriptor's option allows it.
	 */
	bool exclusive;
};

/**
 * @brief A set of selectable options keyed by opaque parser-owned token.
 * The map key is the token the parser addresses by and echoes back verbatim in
 * search(); the consumer never invents or parses it. An option present in the
 * map is in play — a support table lists every offered option, a query carries
 * only the selected ones (there is no separate enabled flag). For an axis with
 * a matching model object (tags), the key equals that object's handle
 * (Tag::ref), so a discovered item round-trips into a query. Each option's
 * human-readable label lives in ItemSelectionValue.
 *
 * @note **Empty in a support table = open vocabulary.** A source may declare an
 *       axis this way (selection semantics — multiple tokens, per-token exclusion,
 *       Tag::ref round-trip) without enumerating its tokens (e.g. millions of tags).
 *       Then any token is accepted: @ref aniparse::validate_search_query checks membership
 *       only against a non-empty (closed) option set. Use TextQuery instead when the
 *       axis is genuine free-text/substring search, not a token identity.
 * @note @ref single_selection is meaningful only in a support table: it limits
 *       a query on this axis to one token. Queries carry the same type as the
 *       support table, but leave it at its default value.
 * @see ItemSelectionValue
 */
struct ItemSelection : std::map<std::string, ItemSelectionValue, std::less<>> {
	using Options = std::map<std::string, ItemSelectionValue, std::less<>>;
	using Options::Options;

	/// Whether this declared axis accepts at most one included or excluded token.
	bool single_selection = false;
};

/**
 * @brief Switch/checkmark. If presented means true
 *        exclusion (if supported).
 */
struct Checkmark {
	bool exclusive = false;
};

/**
 * @brief Any type that can be used for search
 * @see SearchRequestQuery
 */
using SearchItemVariant = std::variant<
    TextQuery,
    IntInterval,
    TimeInterval,
    RelativeTimeInterval,
    ItemSelection,
    Checkmark>;

/**
 * @brief Search key-value used in search() methods.
 * @see search_keys
 */
using SearchItems = std::map<std::string, SearchItemVariant, std::less<>>;

/**
 * @brief Any type that can be used for search
 * @see SearchItemVariant
 * @see search_keys
 */
struct SearchRequestQuery {
	std::string query;
	SearchItems filters;
};

/**
 * @brief One violation found by validate_search_query
 * @see validate_search_query
 */
struct SearchQueryError {
	enum class Reason {
		/// The filter key is not present in the supported set
		UnknownKey,
		/// The filter holds a different SearchItemVariant alternative than declared
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
 * @param supported Declared filters (SearchCompatibilities::supported_filters)
 * @param query The query to check
 * @return Empty if the query is valid; otherwise all violations found
 */
[[nodiscard]] std::vector<SearchQueryError> validate_search_query(
    const SearchItems& supported,
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
 * @brief A source's declared search capability: the filters and sorts it
 * accepts, plus feature flags. A root getter's search_support returns it and
 * validate_query checks a query against it. Domain-agnostic — shared by every
 * getter that searches (manga, images, ...).
 */
struct SearchCompatibilities {
	SearchItems supported_filters;
	SupportedSorts supported_sorts;
	CompatibilitiesFlags compatibilities = compatibilities_flags::default_flags;
	/// When compatibilities carries supports_suggestions, the token kinds the
	/// suggest() @c kind hint accepts — search keys (@see search_keys), the same
	/// vocabulary as supported_filters, with which they typically overlap. Empty
	/// means the source has a single default kind and takes no hint. @see suggest
	std::vector<std::string> supported_suggestion_kinds;
};

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
    const SearchCompatibilities& support,
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
/// Filter by voice-over or subtitle track. Usually ItemSelection or TextQuery.
inline constexpr std::string_view voice           = "voice";
/// Filter by Tag. TextQuery for free-text sources; ItemSelection where the
/// source enumerates its tags, keyed by the opaque token that equals Tag::ref
/// so a tag from MangaInfo searches directly. @see ItemSelection
/// The default @ref aniparse::Tag::axis — a Tag with an empty axis filters through this key.
inline constexpr std::string_view tag             = "tag";
/// Filter by genre — the coarse editorial category axis a source keeps SEPARATE
/// from its finer tag axis (a source with one flat tag list uses @ref tag alone
/// and never this). Usually ItemSelection, keyed like @ref tag. @see ItemSelection
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
/// ItemSelection, or TextQuery.
inline constexpr std::string_view status          = "status";
/// Filter by Rating. Usually IntInterval (a score range) or ItemSelection.
inline constexpr std::string_view rating          = "rating";
/// Filter by year. Usually IntInterval.
inline constexpr std::string_view year            = "year";
/// Filter by AgeRestriction. Usually IntInterval or ItemSelection.
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
/// Filter/suggest by artist or author tag. Usually ItemSelection or TextQuery.
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
// **The value is an ItemSelection with an open vocabulary, not a TextQuery** —
// though a single id looks like free text, it is a token identity, and selection
// semantics are what carry the batch: a selection holds many tokens, and a source
// whose API accepts a set of ids in one call resolves the whole selection in ONE
// request. That is what makes opening fifty saved ids cheap, and it is the reason
// not to "simplify" these keys to a single-string type. Validation of an open
// vocabulary accepts any token (@see ItemSelection). A token IS the id, so an
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
[[nodiscard]] bool is_identity_key(const SearchCompatibilities& support, std::string_view key);
} // namespace aniparse

namespace aniparse {
/**
 * @brief Whether a filter key is an exact id lookup rather than a user-facing filter.
 *
 * Scoped to one source's support table because what counts as a lookup axis is a
 * property of the source, even while the core owns the key names — a caller asks
 * "is this key, on this source, a lookup?". Every caller already holds the table:
 * it is what they are iterating to render or validate.
 *
 * A consumer that presents filters uses this to keep lookups out of the filter
 * surface: they take an id nobody types by hand, they cannot be excluded ('!'),
 * and combining one with a filter is pointless even where the source technically
 * allows it. @see search_keys::identity
 */
[[nodiscard]] bool is_identity_key(const SearchCompatibilities& support, std::string_view key);
} // namespace aniparse
