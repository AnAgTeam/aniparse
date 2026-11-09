#include "Inputs.hpp"
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

namespace {
thread_local bool count_allocations = false;
thread_local std::size_t allocations = 0;
void* allocate(std::size_t size, std::size_t alignment = 0) {
    void* memory = nullptr;
    if (alignment) {
        if (posix_memalign(&memory, alignment, size ? size : 1) != 0) {
            throw std::bad_alloc();
        }
    } else {
        memory = std::malloc(size ? size : 1);
    }
    if (!memory) {
        throw std::bad_alloc();
    }
    if (count_allocations) {
        ++allocations;
    }
    return memory;
}
}

// Counts C++ allocation requests for models and diagnostics. JSON inputs are
// built before counting; Boost.JSON's own direct malloc calls are not counted.
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void* operator new(std::size_t size, std::align_val_t alignment) { return allocate(size, static_cast<std::size_t>(alignment)); }
void* operator new[](std::size_t size, std::align_val_t alignment) { return allocate(size, static_cast<std::size_t>(alignment)); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { std::free(pointer); }

extern "C" {
#define DECLARE(name) int boost_##name(const boost::json::value&); int diagnostic_##name(const boost::json::value&); int lazy_##name(const boost::json::value&);
DECLARE(numbers) DECLARE(array) DECLARE(page) DECLARE(tolerant) DECLARE(strict) DECLARE(variant) DECLARE(tuple) DECLARE(tree)
#undef DECLARE
}

namespace {
using Operation = int(*)(const boost::json::value&);
using Clock = std::chrono::steady_clock;
constexpr int repetitions = 9;
constexpr double sample_seconds = 0.02;

using json_benchmark::page;

struct Sample {
    const char* name;
    boost::json::value input;
    Operation baseline;
    Operation diagnostic;
    Operation lazy;
    bool succeeds = true;
};

double elapsed(Operation operation, const boost::json::value& input, std::size_t iterations) {
    const auto start = Clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        operation(input);
    }
    return std::chrono::duration<double>(Clock::now() - start).count();
}

std::size_t allocation_count(Operation operation, const boost::json::value& input) {
    allocations = 0;
    count_allocations = true;
    operation(input);
    count_allocations = false;
    return allocations;
}
}

int main() {
    boost::json::array array;
    for (int index = 0; index < 1024; ++index) {
        array.push_back(index);
    }
    auto failed_page = page(25);
    failed_page.at("items").at(24).at("authors").at(1).at("name") = 42;
    const auto forgiving = boost::json::parse(R"({"author":{"id":1,"name":42},"id":7})");
    const auto choice = boost::json::parse(R"({"id":7,"name":42})");
    boost::json::value tree{{"children", boost::json::array{}}, {"id", 1}};
    constexpr int tree_depth = 32;
    for (int index = 0; index < tree_depth; ++index) {
        tree = boost::json::object{{"children", boost::json::array{std::move(tree)}}, {"id", index}};
    }
    std::vector<Sample> samples{
        {"numbers", boost::json::parse(R"({"id":7,"score":4.5,"active":true,"counts":[1,2,3,4,5,6,7,8]})"), boost_numbers, diagnostic_numbers, lazy_numbers},
        {"array_1024", std::move(array), boost_array, diagnostic_array, lazy_array},
        {"page_25", page(25), boost_page, diagnostic_page, lazy_page},
        {"page_250", page(250), boost_page, diagnostic_page, lazy_page},
        {"tolerant_discard", forgiving, boost_tolerant, diagnostic_tolerant, lazy_tolerant},
        {"variant_retry", choice, boost_variant, diagnostic_variant, lazy_variant},
        {"tuple", boost::json::parse(R"([1,"owned tuple string long enough for allocation",[1,2,3,4]])"), boost_tuple, diagnostic_tuple, lazy_tuple},
        {"tree_32", std::move(tree), boost_tree, diagnostic_tree, lazy_tree},
        {"failure_nested", std::move(failed_page), boost_page, diagnostic_page, lazy_page, false},
        {"failure_strict", forgiving, boost_strict, diagnostic_strict, lazy_strict, false},
        {"failure_variant", boost::json::value(false), boost_variant, diagnostic_variant, lazy_variant, false},
    };
    bool passed = true;
    std::puts("case,boost_ns,diagnostic_ns,lazy_ns,diagnostic_ratio,lazy_ratio,boost_new,diagnostic_new,lazy_new,iterations");
    for (const auto& sample : samples) {
        constexpr std::size_t mode_count = 3; // Plain Boost, eager and lazy diagnostics.
        const std::array operations{sample.baseline, sample.diagnostic, sample.lazy};
        std::array<std::size_t, mode_count> allocation_counts;
        const int baseline_code = sample.baseline(sample.input);
        for (std::size_t mode = 0; mode < operations.size(); ++mode) {
            const int code = operations[mode](sample.input);
            if (code != baseline_code || ((code == 0) != sample.succeeds)) {
                std::fprintf(stderr, "Result mismatch: %s mode %zu (%d vs %d)\n", sample.name, mode, baseline_code, code);
                passed = false;
            }
            allocation_counts[mode] = allocation_count(operations[mode], sample.input);
            if (sample.succeeds && allocation_counts[mode] != allocation_counts.front()) {
                std::fprintf(stderr, "Extra successful-path allocations: %s mode %zu\n", sample.name, mode);
                passed = false;
            }
        }
        constexpr std::size_t warmup_iterations = 100;
        for (auto operation : operations) {
            elapsed(operation, sample.input, warmup_iterations);
        }
        const auto calibration = elapsed(sample.baseline, sample.input, warmup_iterations);
        const auto iterations = std::clamp(static_cast<std::size_t>(sample_seconds / calibration * warmup_iterations),
            std::size_t{10}, std::size_t{2'000'000});
        std::array<std::vector<double>, mode_count> times;
        for (int round = 0; round < repetitions; ++round) {
            // Rotate order so each implementation runs first equally often.
            for (std::size_t offset = 0; offset < operations.size(); ++offset) {
                const auto mode = (round + offset) % operations.size();
                times[mode].push_back(elapsed(operations[mode], sample.input, iterations));
            }
        }
        std::array<double, mode_count> medians;
        for (std::size_t mode = 0; mode < operations.size(); ++mode) {
            std::sort(times[mode].begin(), times[mode].end());
            medians[mode] = times[mode][repetitions / 2] * 1e9 / iterations;
        }
        std::printf("%s,%.2f,%.2f,%.2f,%.3f,%.3f,%zu,%zu,%zu,%zu\n", sample.name,
            medians[0], medians[1], medians[2], medians[1] / medians[0], medians[2] / medians[0],
            allocation_counts[0], allocation_counts[1], allocation_counts[2], iterations);
    }
    return passed ? 0 : 1;
}
