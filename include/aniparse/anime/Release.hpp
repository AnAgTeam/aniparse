/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"
#include "aniparse/Requests.hpp"

#include <asyncnet/CancellingTask.hpp>
#include <string>
#include <variant>
#include <chrono>
#include <optional>
#include <boost/json.hpp>

namespace aniparse {

using ReleaseFlags = FlagsBitfield<64, struct ReleaseFlagsTag>;

namespace release_flags {

constexpr ReleaseFlags default_flags;
} // namespace release_flags

using ReleaseID             = int64_t;
using EpisodeAgeRestriction = int;
using EpisodeCount          = int;

struct ReleaseRating {
	/// Release rating is from 0 to 10. If the rating is from 0 to 5, just multiple it by 2 (3/5 -> 6/10)
	int rating;
	std::string provider;
};

enum class EpisodeRelativeOperator {
	More,
	Less,
	Exact,
	None
};

struct EpisodeRelativeCount {
	EpisodeCount start               = 0;
	EpisodeRelativeOperator relative = EpisodeRelativeOperator::None;

	constexpr operator bool() const noexcept {
		return relative != EpisodeRelativeOperator::None;
	}
};

enum class ReleaseStatus {
	Unknown,
	Released,
	Ongoing,
	Announced,

};

enum class ReleaseSeason {
	Unknown,
	Spring,
	Summer,
	Fall,
	Winter,

};

struct DubberInfo {
	std::string name;
	std::string social;
};

struct Release {
	ReleaseID id{0};

	std::string title;
	std::string description;

	std::optional<ReleaseSeason> season;
	std::chrono::year year;

	std::optional<EpisodeCount> released_episodes;
	/// optional field, can be casted to bool
	EpisodeRelativeCount total_episodes;
	std::optional<std::chrono::minutes> episode_duration;
	std::optional<EpisodeAgeRestriction> age_restriction;

	std::optional<std::vector<DubberInfo>> dubbers;
	std::optional<ReleaseRating> rating;

	std::variant<std::monostate, boost::json::value, std::shared_ptr<void>, void*> extractor_data;
	ReleaseFlags flags = release_flags::default_flags;
};

class AsyncReleaseGetter {
public:
	virtual ~AsyncReleaseGetter() = default;

	virtual OptionalRequest<std::string> title() const;

protected:
	GetterContext context;
};

} // namespace aniparse