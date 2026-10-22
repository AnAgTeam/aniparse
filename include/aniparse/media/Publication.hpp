/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aniparse {

/**
 * @see AiredStatus::make_default()
 * Default supported AiredStatuses.
 * Used to create generic AiredStatus.
 */
enum class DefaultAiredStatuses {
	Released,  ///< Finished: everything the work will have is out. @see aired_status_released
	Ongoing,   ///< Publishing/airing now, more to come (a hiatus still reads as ongoing). @see aired_status_ongoing
	Announced, ///< Known but nothing released yet. @see aired_status_announced
	/**
	 * Anything the well-known set does not cover — a source-specific state
	 * (cancelled, licensed, on hold), or no state reported at all. A consumer that
	 * needs the distinction reads @ref AiredStatus::name.
	 */
	Other,
};

/// Canonical @ref AiredStatus::name for @ref aniparse::DefaultAiredStatuses::Released.
inline constexpr std::string_view aired_status_released  = "released";
/// Canonical @ref AiredStatus::name for @ref aniparse::DefaultAiredStatuses::Ongoing.
inline constexpr std::string_view aired_status_ongoing   = "ongoing";
/// Canonical @ref AiredStatus::name for @ref aniparse::DefaultAiredStatuses::Announced.
inline constexpr std::string_view aired_status_announced = "announced";

/**
 * @brief How precisely a source dated something — the granularity of a @ref aniparse::ModelDate.
 *
 * A source may give a full calendar day, or only a coarser bucket: a month, a quarter
 * (an anime "season" — Winter/Spring/Summer/Fall map onto Q1..Q4), or a bare year. The
 * date's @ref aniparse::ModelDate::time is pinned to the FIRST instant of that bucket (a year →
 * Jan 1, Q3 → Jul 1), so ordering still works; this field says how much of it is real,
 * so a consumer renders "2025" / "Q1 2025" / "March 2025" / the full date instead of a
 * fabricated January 1.
 */
enum class DatePrecision : std::uint8_t {
	Day,     ///< A full calendar day is known (the default). Render the whole date.
	Month,   ///< Only the month is known — render e.g. "March 2025".
	Quarter, ///< Only the quarter / anime season — @ref aniparse::ModelDate::time is its first day.
	Year,    ///< Only the year is known — render e.g. "2025".
};

/**
 * @brief A moment a source attaches to an item, carrying how precisely it is known.
 *
 * Wrapped rather than a bare time_point so the precision cannot drift from the value.
 * A date field is @c std::optional<ModelDate>: @c nullopt means the source gave no date
 * at all — an absent date is absent, not a sentinel epoch — and a present value is
 * always a real moment plus its @ref aniparse::DatePrecision.
 */
struct ModelDate {
	/// The instant, pinned to the first moment of the precision bucket (@ref aniparse::DatePrecision).
	std::chrono::system_clock::time_point time;
	/// How much of @ref aniparse::ModelDate::time the source actually stated.
	DatePrecision precision = DatePrecision::Day;
};

/**
 * @brief Where an item stands in its publication/airing life: released, ongoing,
 * announced, or something only that source names.
 *
 * The state is carried as a string rather than an enum because the set is open —
 * a source may have states the library never anticipated, and folding them into
 * an "other" bucket at parse time would throw the information away. A consumer
 * that only cares about the common cases calls @ref to_enum; one that wants to
 * show the source's own wording reads @ref name.
 */
struct AiredStatus {
	/// The state, as one of the canonical tokens (@ref aniparse::aired_status_released,
	/// @ref aniparse::aired_status_ongoing, @ref aniparse::aired_status_announced) when it maps onto
	/// one, otherwise the source's own wording. Empty = the source reported no
	/// status; that reads as @ref aniparse::DefaultAiredStatuses::Other, so an item whose
	/// status is unknown is not silently reported as released.
	std::string name;
	/// The moment the source attaches to that state, when it gives one. @c nullopt (the
	/// default) when the source states only the state and no date — the common case, so
	/// treat a date here as a bonus and never as a reliable ordering key. Not to be
	/// confused with the item's own release_time.
	std::optional<ModelDate> time;

	/**
	 * @brief Classify @ref name into the well-known set.
	 * @return The matching @ref aniparse::DefaultAiredStatuses, or DefaultAiredStatuses::Other
	 *         for a source-specific or empty name. Pure string match against the
	 *         canonical tokens — no locale or case folding.
	 */
	DefaultAiredStatuses to_enum() const;
};


/**
 * @brief The calendar unit of a recurring release interval.
 *
 * Together with @ref RecurringReleaseInfo::interval, this preserves a source's
 * declared cadence without turning calendar months into an approximate number
 * of days.
 */
enum class ReleaseCadenceUnit {
	Days,   ///< Repeats after a whole-number count of calendar days.
	Weeks,  ///< Repeats after a whole-number count of calendar weeks.
	Months, ///< Repeats after a whole-number count of calendar months.
};

/**
 * @brief A source-declared recurring publication or airing schedule.
 *
 * This describes a cadence, not an inferred next installment. Sources may
 * publish only the interval, a weekday, a time of day, or any combination of
 * them. Use a domain's upcoming-installment field for a concrete next release.
 */
struct RecurringReleaseInfo {
	/// Positive number of @ref unit units between releases. Parsers must leave
	/// the enclosing optional empty rather than emit zero.
	unsigned int interval = 1;
	/// Calendar unit paired with @ref interval. Weeks is the neutral default for
	/// sources that state only a broadcast weekday.
	ReleaseCadenceUnit unit = ReleaseCadenceUnit::Weeks;
	/// Day on which a weekly cadence occurs. @c nullopt = the source does not
	/// name one, including non-weekly cadences. `std::chrono::weekday` uses
	/// Sunday = 0, Monday = 1 through Saturday = 6.
	std::optional<std::chrono::weekday> weekday;
	/// Local time of day announced by the source. @c nullopt = it names no time;
	/// no timezone is implied by this value.
	std::optional<std::chrono::minutes> time_of_day;
};

} // namespace aniparse
