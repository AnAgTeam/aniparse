/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include <array>
#include <optional>
#include <span>

namespace aniparse {

/**
 * Structure representing rating (score) of the item (release, manga, etc.)
 *
 * Sources score on incompatible scales (5 stars, 0-10, a 0-100 mean), so the value
 * is stored normalized: @ref score is always on a 0-10 scale, whatever the source
 * used, and a consumer can compare or render it without knowing where it came from.
 * @ref max_score keeps the source's own scale for callers that want to show the raw
 * "8/10" or "83/100" form.
 */
class Rating {
public:
	/**
	 * @brief Construct rating from general information
	 * @param score Score [1, max]
	 * @param max_score Maximum score value
	 * @param raters Optionally, total count of votes
	 * @return rating
	 */
	static Rating from_score(double score, int max_score, std::optional<int> raters = std::nullopt) noexcept;

	/**
	 * @brief Construct rating from distrubution.
	 *        Other values i.e. 'score' is calculated from it.
	 * @param distribution Array of vote counts starting from 1, where distribution[i] = i vote count
	 * @throw Logic error if distribution.size() > 10
	 * @return rating
	 */
	static Rating from_distribution(std::span<const int> distribution);

	/**
	 * @brief Copy. A rating is a small value type, held by the item that carries it.
	 * @param other Rating to copy.
	 */
	Rating(const Rating& other) noexcept = default;
	/**
	 * @brief Move. Equivalent to a copy — the payload is trivially copyable.
	 * @param other Rating to move from; left valid and unchanged.
	 */
	Rating(Rating&& other) noexcept      = default;
	~Rating() noexcept                   = default;

	/**
	 * @brief Copy-assign.
	 * @param other Rating to copy.
	 * @return This rating.
	 */
	Rating& operator=(const Rating& other) noexcept = default;
	/**
	 * @brief Move-assign.
	 * @param other Rating to move from; left valid and unchanged.
	 * @return This rating.
	 */
	Rating& operator=(Rating&& other) noexcept      = default;

	/**
	 * @brief The score, rescaled to a common 0-10 axis whatever the source's own
	 * scale was, so ratings from different sources are directly comparable.
	 * @return The normalized score in [0, 10]. Multiply back by
	 *         @ref max_score / 10 to recover the value the source published.
	 */
	double score() const noexcept;

	/**
	 * @brief The source's own scale, kept only so the raw form can be shown.
	 * @return The maximum the source scores out of — as it publishes it (5, 10, 100),
	 *         not rescaled. This is NOT the range of @ref score.
	 */
	int max_score() const noexcept;

	/**
	 * @brief How many votes the score averages, when the source publishes it.
	 * @return The vote count, or nullopt when the source reports a score without a
	 *         vote count (so a lone 10/10 cannot be told from a well-established one).
	 */
	std::optional<int> raters() const noexcept;

	/**
	 * @brief The per-bucket vote breakdown, for sources that publish a histogram.
	 * @return Vote counts indexed from score 1 upward (element i = votes for score
	 *         i+1), spanning @ref max_score buckets; nullopt when the rating came
	 *         from a plain average, which is the common case. The span borrows from
	 *         this Rating and dies with it — copy it out to keep it.
	 */
	std::optional<std::span<const int>> distribution() const noexcept;

private:
	Rating() noexcept = default;

	double score_              = 0;
	int max_score_             = 0;
	std::optional<int> raters_ = 0;
	std::optional<std::array<int, 10>> distribution_;
};

/**
 * @brief How much attention an item has had on its source.
 * Its own struct rather than a bare int so an absent count (an optional ViewStats
 * left empty) is distinguishable from a real zero, and so further counters can be
 * added without breaking the field's users.
 */
struct ViewStats {
	/// Views the source reports, in whatever way it counts them — a source-defined
	/// number that is comparable between items of ONE source and meaningless across
	/// sources. 0 = genuinely none reported; an item whose source publishes no view
	/// count carries no ViewStats at all instead.
	int views = 0;
};

} // namespace aniparse
