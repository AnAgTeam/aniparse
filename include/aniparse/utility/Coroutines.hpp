/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <coro/when_all.hpp>
#include <coro/task.hpp>

#ifdef _MSC_VER
# pragma warning(push)
//#pragma warning(disable : 26800) // Disable "Usage of already moved object" for gather_awaitables()
#else
 // TODO
#endif

namespace aniparse {
	namespace detail {
		/// coro::when_all awaitable, that can be co_await'ed
		template<coro::concepts::awaitable ... Awaitables>
		using WhenAllAwaitable = decltype(coro::when_all(std::declval<Awaitables>() ...));

		//template<coro::concepts::awaitable ... Awaitables>
		//using WhenAllTest = std::invoke_result_t<decltype(coro::when_all<Awaitables ...>), Awaitables ...>;

		/// Tuple with return values from coro::when_all's task
		template<coro::concepts::awaitable ... Awaitables>
		using WhenAllAwaitableTuple = std::remove_reference_t<decltype(std::declval<typename coro::concepts::awaitable_traits<WhenAllAwaitable<Awaitables ...>>::awaiter_type>().await_resume())>;

		/**
		 * Class that holds coro::when_all tasks alive
		 * and acts as std::tuple
		 * @tparam Awaitables ... Awaitable types, that all will be co_await'ed
		 */
		template<coro::concepts::awaitable ... Awaitables>
		struct GatherTasksStorage {
			/**
			 * @brief construct storage
			 * @param task coro::when_all task
			 */
			GatherTasksStorage(WhenAllAwaitable<Awaitables ...>&& task) : when_all_task(std::move(task)) {}

			/// disable copy for safety
			GatherTasksStorage(GatherTasksStorage& other) = delete;
			/// disable move for safety
			GatherTasksStorage(GatherTasksStorage&& other) = delete;

			/// disable copy for safety
			GatherTasksStorage& operator=(GatherTasksStorage& other) = delete;
			/// disable move for safety
			GatherTasksStorage& operator=(GatherTasksStorage&& other) = delete;

			WhenAllAwaitable<Awaitables ...> when_all_task;
		};

		/**
		 * Helper class to fix behavior when std::get
		 * are called after the task if destroyed
		 * leading to undefined behaviour. The class
		 * doesn't returns r-value tuple with values,
		 * intead, it moves task (With coroutine_handle's and a latch)
		 * to another class, that acts as tuple.
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
#if defined(_DEBUG) || defined(ANIPARSE_BUILD_IS_TESTS)
				assert(!moved);
				moved = true;
#endif
				return GatherTasksStorage<Awaitables ...>(std::move(when_all_task));
			}

			WhenAllAwaitable<Awaitables ...> when_all_task;
#if defined(_DEBUG) || defined(ANIPARSE_BUILD_IS_TESTS)
			bool moved = false;
#endif
		};

		/**
		 * @brief Get GatherTasksStorage reslt for task at index
		 * @tparam Index of task to get value
		 * @tparam Awaitables ... awaitables types of storage
		 * @param storage R-Value storage with tasks
		 * @returns Moved task value from at 'Index'
		 */
		template<size_t Index, typename ... Awaitables>
		auto get(GatherTasksStorage<Awaitables ...>&& storage) {
			using ReturnType = std::tuple_element_t<Index, GatherTasksStorage<Awaitables ...>>;
			auto&& result_value = std::get<Index>(coro::concepts::get_awaiter(std::move(storage.when_all_task)).await_resume()).return_value();
			return static_cast<ReturnType&&>(result_value);
		}
	}

	/**
	 * @brief co_await all awaitables and return its return values
	 * The function returns only when *all* tasks are finished
	 * @tparam Awaitables ... Awaitable types, that will be co_await'ed
	 * @param awaitables Awaitables, that will be co_await'ed
	 * @return Awaitable, when co_await'ed returns tuple-like object
	 *         with all the values from 'awaitables'
	 */
	template<coro::concepts::awaitable ... Awaitables>
	auto gather_awaitables(Awaitables ... awaitables) {
		return detail::GatherAwaitable<Awaitables ...>{
			coro::when_all(std::move(awaitables) ...)
		};
	}
}

/// Total results count of GatherTasksStorage
template<coro::concepts::awaitable ... Awaitables>
struct std::tuple_size<aniparse::detail::GatherTasksStorage<Awaitables ...>> :
	std::integral_constant<size_t, sizeof...(Awaitables)> {};

/// Return type of get(GatherTasksStorage) function
template<size_t Index, coro::concepts::awaitable ... Awaitables>
struct std::tuple_element<Index, aniparse::detail::GatherTasksStorage<Awaitables ...>> {
	//static_assert(Index < sizeof...(Awaitables), "Invalid index for GatherAwaitable");

	using type = std::remove_reference_t<decltype(std::declval<std::tuple_element_t<Index, aniparse::detail::WhenAllAwaitableTuple<Awaitables ...>>>().return_value())>;
};

//template<size_t Index, coro::concepts::awaitable ... Awaitables>
//struct std::tuple_element<Index, aniparse::detail::GatherTasksStorage<Awaitables ...>> :
//	std::tuple_element<Index, std::tuple<std::remove_reference_t<typename coro::concepts::awaitable_traits<Awaitables>::awaiter_return_type> ...>> {
//	static_assert(Index < sizeof...(Awaitables), "Invalid index for GatherAwaitable");
//};

#ifdef _MSC_VER
# pragma warning(pop)
#else
// TODO
#endif