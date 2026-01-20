/**
 * Closure and Iterator Benchmarks (C++)
 *
 * Tests closure overhead: lambda capture, function pointers, iterators.
 */

#include "../common/bench.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <numeric>
#include <vector>

volatile int64_t sink = 0;

// ============================================================================
// Lambda / Closure Benchmarks
// ============================================================================

// Lambda no capture
void bench_lambda_no_capture(int64_t iterations) {
    auto f = [](int64_t x) { return x * 2; };
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += f(i);
    }
    sink = sum;
}

// Lambda value capture
void bench_lambda_value_capture(int64_t iterations) {
    int64_t multiplier = 3;
    auto f = [multiplier](int64_t x) { return x * multiplier; };
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += f(i);
    }
    sink = sum;
}

// Lambda reference capture
void bench_lambda_ref_capture(int64_t iterations) {
    int64_t multiplier = 3;
    auto f = [&multiplier](int64_t x) { return x * multiplier; };
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += f(i);
    }
    sink = sum;
}

// Lambda multi-capture
void bench_lambda_multi_capture(int64_t iterations) {
    int64_t a = 1, b = 2, c = 3, d = 4;
    auto f = [a, b, c, d](int64_t x) { return x + a + b + c + d; };
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += f(i);
    }
    sink = sum;
}

// std::function wrapper (type erased)
void bench_std_function(int64_t iterations) {
    std::function<int64_t(int64_t)> f = [](int64_t x) { return x * 2; };
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += f(i);
    }
    sink = sum;
}

// Higher-order function (passing lambda)
template <typename F> int64_t apply_n_times(F f, int64_t x, int64_t n) {
    int64_t result = x;
    for (int64_t i = 0; i < n; ++i) {
        result = f(result);
    }
    return result;
}

void bench_higher_order(int64_t iterations) {
    auto f = [](int64_t x) { return x + 1; };
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += apply_n_times(f, i % 100, 5);
    }
    sink = sum;
}

// Closure returning closure
void bench_closure_factory(int64_t iterations) {
    auto make_adder = [](int64_t n) { return [n](int64_t x) { return x + n; }; };

    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto add_i = make_adder(i % 100);
        sum += add_i(i);
    }
    sink = sum;
}

// ============================================================================
// Iterator Benchmarks
// ============================================================================

// Manual loop vs range-based for
void bench_manual_loop(int64_t iterations) {
    std::vector<int64_t> vec(1000);
    std::iota(vec.begin(), vec.end(), 0);

    int64_t total = 0;
    for (int64_t iter = 0; iter < iterations / 1000; ++iter) {
        for (size_t i = 0; i < vec.size(); ++i) {
            total += vec[i];
        }
    }
    sink = total;
}

void bench_iterator_loop(int64_t iterations) {
    std::vector<int64_t> vec(1000);
    std::iota(vec.begin(), vec.end(), 0);

    int64_t total = 0;
    for (int64_t iter = 0; iter < iterations / 1000; ++iter) {
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            total += *it;
        }
    }
    sink = total;
}

void bench_range_for(int64_t iterations) {
    std::vector<int64_t> vec(1000);
    std::iota(vec.begin(), vec.end(), 0);

    int64_t total = 0;
    for (int64_t iter = 0; iter < iterations / 1000; ++iter) {
        for (const auto& val : vec) {
            total += val;
        }
    }
    sink = total;
}

// STL algorithms
void bench_std_for_each(int64_t iterations) {
    std::vector<int64_t> vec(1000);
    std::iota(vec.begin(), vec.end(), 0);

    int64_t total = 0;
    for (int64_t iter = 0; iter < iterations / 1000; ++iter) {
        std::for_each(vec.begin(), vec.end(), [&total](int64_t x) { total += x; });
    }
    sink = total;
}

void bench_std_accumulate(int64_t iterations) {
    std::vector<int64_t> vec(1000);
    std::iota(vec.begin(), vec.end(), 0);

    int64_t total = 0;
    for (int64_t iter = 0; iter < iterations / 1000; ++iter) {
        total += std::accumulate(vec.begin(), vec.end(), 0LL);
    }
    sink = total;
}

void bench_std_transform(int64_t iterations) {
    std::vector<int64_t> src(1000);
    std::vector<int64_t> dst(1000);
    std::iota(src.begin(), src.end(), 0);

    for (int64_t iter = 0; iter < iterations / 1000; ++iter) {
        std::transform(src.begin(), src.end(), dst.begin(), [](int64_t x) { return x * 2; });
    }
    sink = dst[0];
}

void bench_std_filter(int64_t iterations) {
    std::vector<int64_t> vec(1000);
    std::iota(vec.begin(), vec.end(), 0);

    int64_t total = 0;
    for (int64_t iter = 0; iter < iterations / 1000; ++iter) {
        for (const auto& x : vec) {
            if (x % 2 == 0) {
                total += x;
            }
        }
    }
    sink = total;
}

// Chain of operations
void bench_chain_operations(int64_t iterations) {
    std::vector<int64_t> vec(1000);
    std::iota(vec.begin(), vec.end(), 0);

    int64_t total = 0;
    for (int64_t iter = 0; iter < iterations / 1000; ++iter) {
        // filter -> map -> fold
        for (const auto& x : vec) {
            if (x % 2 == 0) {
                total += x * 2;
            }
        }
    }
    sink = total;
}

int main() {
    bench::Benchmark b("Closures and Iterators");

    const int64_t ITERATIONS = 10000000; // 10M

    // Closure benchmarks
    b.run_with_iter("Lambda No Capture", ITERATIONS, bench_lambda_no_capture, 10);
    b.run_with_iter("Lambda Value Capture", ITERATIONS, bench_lambda_value_capture, 10);
    b.run_with_iter("Lambda Ref Capture", ITERATIONS, bench_lambda_ref_capture, 10);
    b.run_with_iter("Lambda Multi Capture", ITERATIONS, bench_lambda_multi_capture, 10);
    b.run_with_iter("std::function Wrapper", ITERATIONS, bench_std_function, 10);
    b.run_with_iter("Higher Order Function", ITERATIONS, bench_higher_order, 10);
    b.run_with_iter("Closure Factory", ITERATIONS, bench_closure_factory, 10);

    // Iterator benchmarks
    b.run_with_iter("Manual Loop (index)", ITERATIONS, bench_manual_loop, 10);
    b.run_with_iter("Iterator Loop", ITERATIONS, bench_iterator_loop, 10);
    b.run_with_iter("Range-based For", ITERATIONS, bench_range_for, 10);
    b.run_with_iter("std::for_each", ITERATIONS, bench_std_for_each, 10);
    b.run_with_iter("std::accumulate", ITERATIONS, bench_std_accumulate, 10);
    b.run_with_iter("std::transform", ITERATIONS, bench_std_transform, 10);
    b.run_with_iter("Filter Pattern", ITERATIONS, bench_std_filter, 10);
    b.run_with_iter("Chain Operations", ITERATIONS, bench_chain_operations, 10);

    b.print_results();
    b.save_json("../results/closure_cpp.json");

    return 0;
}
