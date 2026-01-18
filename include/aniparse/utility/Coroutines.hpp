#pragma once
#include <coro/when_all.hpp>

namespace aniparse {
	namespace detail {
		template<
			typename T,
			size_t ... Indexes>
		inline auto gather_make_tuple(
			std::index_sequence<Indexes ...>,
			T&& tuple_val) {
			return std::make_tuple(
				std::move(std::get<Indexes>(std::forward<T>(tuple_val)).return_value()) ...);
		}

		/**
		 * Helper class to fix behavior with common
		 * coro::task that returns r-value reference,
		 * then the task gets destroyed, and then
		 * awaiter calls std::get<N>(...) on the r-value
		 * reference tuple, so it's undefined behavior.
		 * The following code with coro::task<tuple<...>> crashes:
		 *   auto [r1, r2] = gather(coro1(), coro2());
		 *
		 * In addition, it isn't creating any other promise object
		 * on heap.
		 * @tparam Awaitables ... Awaitable types, that all will be co_await'ed
		 */
		template<coro::concepts::awaitable ... Awaitables>
		struct GatherAwaitable {

			bool await_ready() {
				return sizeof...(Awaitables) == 0;
			}

			auto await_suspend(std::coroutine_handle<> awaiting_coroutine) {
				return coro::concepts::get_awaiter(when_all_task).await_suspend(awaiting_coroutine);
			}

			auto await_resume() {
				return gather_make_tuple(
					std::make_index_sequence<sizeof...(Awaitables)>{},
					std::move(coro::concepts::get_awaiter(when_all_task).await_resume())
				);
			}

			decltype(coro::when_all(Awaitables{} ...)) when_all_task;
		};
	}

	template<coro::concepts::awaitable ... Awaitables>
	auto gather_awaitables(Awaitables ... awaitables) {
		return detail::GatherAwaitable<Awaitables ...>{
			coro::when_all(std::move(awaitables) ...)
		};
	}
}