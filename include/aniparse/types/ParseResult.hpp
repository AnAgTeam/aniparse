/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/utility/FlagsBitfield.hpp"

/**
 * @file
 * The envelope for a parse: a payload plus what the source said about the query
 * that produced it (@ref aniparse::ParseResult, @ref aniparse::ParseQueryResult). Distinct from the
 * capability flags of types/Flags.hpp, which describe a source up front — these
 * describe one answer, after the fact.
 *
 * @note Reserved surface. No getter of the current API returns a ParseResult; it
 * exists so a parse entry point can gain per-answer metadata without changing the
 * shape of its payload.
 */

namespace aniparse {

/**
 * @brief Per-answer flags, drawn from the parse_flags namespace.
 * A strongly typed 64-bit flag word, distinct from CompatibilitiesFlags so a
 * capability bit and a result bit can never be confused for one another.
 */
using ParseFlags = FlagsBitfield<64, struct ParseFlagsTag>;

/**
 * @brief The flags a parse can report back about the query it answered.
 * @see ParseFlags, ParseQueryResult
 */
namespace parse_flags {
/**
 * Reserved. No facility of the library sets or reads this bit today; it carries
 * no defined contract for a consumer.
 */
constexpr auto supports_inplace_get = ParseFlags::make_bit(0);

/// The empty set — the parse reports nothing beyond its payload. The initial
/// value of ParseQueryResult::flags.
constexpr ParseFlags default_flags;
} // namespace parse_flags

/**
 * @brief What a source reported about the query, alongside (not inside) the data
 * it returned.
 *
 * The metadata half of @ref ParseResult — it describes the answer, so it is
 * filled in by the parser that produced it and is read-only for the caller.
 */
struct ParseQueryResult {
	/// Whatever the parse chose to report about this answer; empty by default.
	ParseFlags flags = parse_flags::default_flags;
};

/**
 * @brief A parsed payload together with the metadata of the query that produced
 * it.
 *
 * Keeps the metadata out of the payload type, so the same data model @p T serves
 * a call that cares about the query's outcome and one that only wants the items.
 * The result owns @c data outright; the caller may move it out.
 *
 * @tparam T The parsed payload type (an info model, a page of items, …).
 */
template <typename T>
struct ParseResult {
	/// The parsed payload, owned by this result.
	T data;
	/// What the source reported about the query. Empty flags when it reported
	/// nothing.
	ParseQueryResult meta;
};

} // namespace aniparse
