/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include "aniparse/media/Text.hpp"
#include "aniparse/media/Publication.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aniparse {

/**
 * @brief A person or account credited on an item: an author, an artist, an
 * uploader, a translation team, the writer of a comment.
 *
 * A person is not modelled as a full entity — the library carries the name to show
 * plus an opaque handle to ask the source for more, and nothing else.
 */
struct RelatedUser {
	/// Display name as the source writes it. Empty = the source credits nobody in
	/// this role (an anonymous upload, an unattributed work) — it is not a promise
	/// that the role is inapplicable.
	std::string name;
	/// Opaque parser-owned reference; @see Tag::ref. Empty = the source exposes no
	/// handle for this person, so only the name is available.
	std::string ref;
};

/**
 * @see UserList::make_default()
 * Default supported UserLists.
 * Used to create generic UserList.
 */
enum class DefaultUserLists {
	Planning, ///< Queued to read/watch later. @see user_list_planning
	Dropped,  ///< Abandoned before finishing. @see user_list_dropped
	Favorite, ///< Marked a favourite; orthogonal to progress on some sources. @see user_list_favorite
	/// A list the well-known set does not name (e.g. "on hold", or a user-made
	/// list). The source's own wording stays in @ref UserList::name.
	Other,
	// video related
	Watched,  ///< Finished watching. @see user_list_watched
	Watching, ///< Currently watching. @see user_list_watching
	// book related
	Reading,  ///< Currently reading. @see user_list_reading
	Read,     ///< Finished reading. @see user_list_read
	/// Temporarily paused, without abandoning the work. @see user_list_on_hold
	/// Kept last to preserve the numeric values of the older public enum cases.
	OnHold,
};

/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Planning.
inline constexpr std::string_view user_list_planning = "planning";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Dropped.
inline constexpr std::string_view user_list_dropped  = "dropped";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Favorite.
inline constexpr std::string_view user_list_favorite = "favorite";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Watched.
inline constexpr std::string_view user_list_watched  = "watched";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Watching.
inline constexpr std::string_view user_list_watching = "watching";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Reading.
inline constexpr std::string_view user_list_reading  = "reading";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::Read.
inline constexpr std::string_view user_list_read     = "read";
/// Canonical @ref UserList::name for @ref aniparse::DefaultUserLists::OnHold.
inline constexpr std::string_view user_list_on_hold  = "on_hold";

/**
 * Structure representing list, that users added
 * the item. Usually in release, manga headers
 *
 * The list belongs to the account the request was made with, so it is meaningful
 * only on an authenticated source; an anonymous fetch cannot know it. Like
 * @ref AiredStatus the name is an open string, because a source may keep lists the
 * library does not name.
 */
struct UserList {
	/// The list, as one of the canonical tokens (@ref aniparse::user_list_reading, …) when it
	/// maps onto one, otherwise the source's own wording. Empty reads as
	/// @ref aniparse::DefaultUserLists::Other.
	std::string name;

	/**
	 * @brief Classify @ref name into the well-known set.
	 * @return The matching @ref aniparse::DefaultUserLists, or DefaultUserLists::Other for a
	 *         source-specific or empty name.
	 */
	DefaultUserLists to_enum() const;
};

/**
 * @brief A public count of users who placed one work in a named list.
 *
 * This is aggregate, source-published popularity data for the work, not the
 * authenticated user's own list membership (@ref UserList). A source may expose
 * only some list categories, so consumers render the entries supplied rather
 * than synthesising missing zeroes.
 */
struct AudienceListCount {
	/// The list's source spelling, using a canonical @ref UserList::name token when
	/// it maps to one. Never empty for an entry emitted by a parser.
	std::string name;
	/// Number of source users in @ref name. @c nullopt = the source identifies the
	/// category but withholds its count; zero is a real reported count.
	std::optional<long> users_count;
};

/**
 * @brief A user comment on an item (manga / anime / …). Available when the parser
 * advertises capabilities_flags::supports_commenting.
 *
 * A single flat comment, or the root of a thread via @ref replies. Top-level
 * comments paginate through the getter's GetFilters like any other list; deep
 * reply pagination, when a source has it, is a later follow-up keyed by @ref ref.
 */
struct Comment {
	/// Who wrote it. Its name is empty for an anonymous post; its ref, when the
	/// source exposes one, addresses the account. @see RelatedUser
	RelatedUser author;
	/// The body, with any links the source marked up preserved as attributes.
	AttributedText text;
	/// When it was posted; @c nullopt when the source does not date it
	/// (some report only a relative "2 days ago" the parser will not guess from).
	std::optional<ModelDate> time;
	/// Net score / likes, if the source exposes one. Source-defined and possibly
	/// negative where downvotes exist. nullopt = the source has no voting on
	/// comments, or does not report it — never "zero votes".
	std::optional<int> score;
	/// Inline thread replies; empty when the source is flat or the comment has none.
	std::vector<Comment> replies;
	/// Opaque parser-owned handle; @see Tag::ref. Round-trips to fetch this
	/// comment's replies when a source paginates threads separately.
	std::string ref;
};

} // namespace aniparse
