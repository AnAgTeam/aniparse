#include <aniparse/AniParse.hpp>
#include <aniparse/ParserStore.hpp>
#include <aniparse/DomainScanner.hpp>
#include <aniparse/utility/Coroutines.hpp>

#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <span>
#include <optional>

#include <print>
#include <fstream>
#include <filesystem>

#include <iostream>
#include <string>
#include <coro/task.hpp>
#include <coro/sync_wait.hpp>

using namespace aniparse;


namespace test_ns {
    struct MyType {};



    template<size_t Index, typename ... T>
    size_t get(test_ns::MyType&& type) {
        return Index;
    }
}

template<>
struct std::tuple_size<test_ns::MyType> : std::integral_constant<size_t, 2> {};

template<size_t Index>
struct std::tuple_element<Index, test_ns::MyType> {
    using type = int;
};

coro::task<void> test_coroutines() {
    struct TestType {
        TestType(int value) : value(value) {
            std::println("TestType ctx: {}", value);
        }
        TestType(TestType&& other) : value(other.value) {
            std::println("TestType&& move");
        }
        TestType(const TestType& other) : value(other.value) {
            std::println("TestType& copy");
        }
        ~TestType() {
            std::println("~TestType()");
        }

        int value;
    };

    auto test_coro = []() -> coro::task<TestType> {
        co_return 10;
    };

    auto test_coro2 = []() -> coro::task<TestType> {
        TestType out = 20;
        co_return out;
    };

    auto test_coro3 = []() -> coro::task<void> {
        co_return;
    };

    //auto my_type = test_ns::MyType{};
    //auto [a1, a2] = my_type;
    //std::println("{}, {}", a1, a2);

    auto gather_await = co_await gather_awaitables(test_coro(), test_coro2());
    constexpr auto gsize = std::tuple_size<decltype(gather_await)>::value;
    using Ty = typename std::tuple_element<1, decltype(gather_await)>::type;
    
    auto val1 = get<0>(std::move(gather_await));
    auto val2 = get<1>(std::move(gather_await));

    //auto [v1, v2] = std::move(gather_await);

    std::tuple<int, double> test_tuple;
    auto tuple_val1 = std::get<0>(std::move(test_tuple));
    auto tuple_val11 = std::get<0>(std::move(test_tuple));
    auto tuple_val2 = std::get<1>(std::move(test_tuple));

    auto [ca1, ca2] = co_await gather_awaitables(test_coro(), test_coro2());

    auto [task1, task2] = co_await gather_awaitables(test_coro(), test_coro3());
    std::println("{}, {}", ca1.value, ca2.value);

    co_return;
}

int main() {
    coro::sync_wait(test_coroutines());
}