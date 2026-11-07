/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "aniparse/detail/PublishedFlight.hpp"

#include <utility>

namespace aniparse::detail {

PublishedFlightBase::Waiter::Waiter(std::shared_ptr<PublishedFlightBase> flight)
    : flight_(std::move(flight)) {}

PublishedFlightBase::Waiter::~Waiter() {
    std::lock_guard lock(flight_->mutex_);
    if (queued_) flight_->waiters_.erase(position_);
}

bool PublishedFlightBase::Waiter::await_ready() const noexcept { return false; }

bool PublishedFlightBase::Waiter::await_suspend(std::coroutine_handle<> continuation) {
    std::lock_guard lock(flight_->mutex_);
    if (flight_->completed_) return false;
    continuation_ = continuation;
    position_ = flight_->waiters_.insert(flight_->waiters_.end(), this);
    queued_ = true;
    return true;
}

void PublishedFlightBase::Waiter::await_resume() const noexcept {}

void PublishedFlightBase::complete() noexcept {
    std::unique_lock lock(mutex_);
    completed_ = true;
    // Pop individually: a resumed caller may destroy another waiting task.
    // Never hold the state lock while executing arbitrary continuations.
    while (!waiters_.empty()) {
        auto* waiter = waiters_.front();
        waiters_.pop_front();
        waiter->queued_ = false;
        auto continuation = waiter->continuation_;
        lock.unlock();
        continuation.resume();
        lock.lock();
    }
}

} // namespace aniparse::detail
