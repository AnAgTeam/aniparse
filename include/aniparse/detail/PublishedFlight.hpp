/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/utility/Result.hpp"

#include <coroutine>
#include <exception>
#include <list>
#include <memory>
#include <mutex>

namespace aniparse::detail {

// Synchronization independent of the published payload. A suspended waiter
// unregisters when destroyed. Task owners must still serialize destruction
// against execution/resumption of that same task.
class PublishedFlightBase {
public:
    class Waiter {
    public:
        explicit Waiter(std::shared_ptr<PublishedFlightBase> flight);
        Waiter(const Waiter&) = delete;
        Waiter(Waiter&&) = delete;
        ~Waiter();

        bool await_ready() const noexcept;
        bool await_suspend(std::coroutine_handle<> continuation);
        void await_resume() const noexcept;

    private:
        friend class PublishedFlightBase;
        std::shared_ptr<PublishedFlightBase> flight_;
        std::coroutine_handle<> continuation_;
        std::list<Waiter*>::iterator position_;
        bool queued_ = false;
    };

    void complete() noexcept;

protected:
    PublishedFlightBase() = default;
    ~PublishedFlightBase() = default;

private:
    std::mutex mutex_;
    bool completed_ = false;
    std::list<Waiter*> waiters_;
};

// One immutable, typed outcome per fetch attempt. Shared ownership retains the
// concrete instance even when a waiter holds only its synchronization base.
template <class T>
class PublishedFlight : public PublishedFlightBase {
public:
    using Outcome = Result<std::shared_ptr<const T>>;

    // Prepared before starting I/O, so abandoning the producer needs no allocation.
    Outcome outcome = make_request_error(RequestErrorCode::Cancelled, "Cache load abandoned");
    std::exception_ptr exception;

    Outcome result() const {
        if (exception) std::rethrow_exception(exception);
        return outcome;
    }
};

} // namespace aniparse::detail
