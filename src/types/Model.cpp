#include "aniparse/types/Model.hpp"

#include <numeric>

namespace aniparse {
Rating Rating::from_score(
	double score,
	int max_score,
	std::optional<int> raters) noexcept {
	Rating rating;
	rating.score_     = max_score != 0 ? score / max_score * 10.0 : 0;
	rating.max_score_ = max_score;
	rating.raters_ = raters;
	return rating;
}

Rating Rating::from_distribution(std::span<const int> distribution) {
	if (distribution.size() > 10) {
		throw std::logic_error("Rating distribution cannot be more than 10");
	}

	double weighted = std::accumulate(distribution.begin(), distribution.end(), 0.0,
	                                  [i = 1](double prev, double current) mutable {
		                                  return prev + current * (i++);
	                                  });

	Rating rating;
	rating.max_score_ = static_cast<int>(distribution.size()); // cannot be > 10
	rating.score_     = weighted  / static_cast<double>(distribution.size());
	rating.raters_    = std::accumulate(distribution.begin(), distribution.end(), 0);
	// create array and copy distrubution to it. Other values stays 0
	rating.distribution_ = std::array<int, 10>{};
	std::copy(distribution.begin(), distribution.end(), rating.distribution_->begin());
	return rating;
}

double Rating::score() const noexcept {
	return score_;
}

int Rating::max_score() const noexcept {
	return max_score_;
}

std::optional<int> Rating::raters() const noexcept {
	return raters_;
}

std::optional<std::span<const int>> Rating::distribution() const noexcept {
	if (!distribution_) {
		return std::nullopt;
	}
	return std::span<const int>(distribution_->begin(), distribution_->begin() + max_score_);
}

DefaultAiredStatuses AiredStatus::to_enum() const {
	if (name == aired_status_released) {
		return DefaultAiredStatuses::Released;
	}
	if (name == aired_status_ongoing) {
		return DefaultAiredStatuses::Ongoing;
	}
	if (name == aired_status_announced) {
		return DefaultAiredStatuses::Announced;
	}
	return DefaultAiredStatuses::Other;
}

DefaultUserLists UserList::to_enum() const {
	if (name == user_list_planning) {
		return DefaultUserLists::Planning;
	}
	if (name == user_list_dropped) {
		return DefaultUserLists::Dropped;
	}
	if (name == user_list_favorite) {
		return DefaultUserLists::Favorite;
	}
	if (name == user_list_watched) {
		return DefaultUserLists::Watched;
	}
	if (name == user_list_watching) {
		return DefaultUserLists::Watching;
	}
	if (name == user_list_reading) {
		return DefaultUserLists::Reading;
	}
	if (name == user_list_read) {
		return DefaultUserLists::Read;
	}
	if (name == user_list_on_hold) {
		return DefaultUserLists::OnHold;
	}
	return DefaultUserLists::Other;
}
} // namespace aniparse
