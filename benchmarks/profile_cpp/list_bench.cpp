/**
 * List/Vec Benchmarks (C++)
 *
 * Tests std::vector operations: push_back, pop_back, operator[], iteration.
 * Matches TML list_bench.tml and Rust list_bench.rs for direct comparison.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <vector>

// Prevent optimization
volatile int64_t sink = 0;

// Vector push_back (grow from empty)
void bench_vec_push(int64_t iterations) {
    std::vector<int64_t> vec;
    for (int64_t i = 0; i < iterations; ++i) {
        vec.push_back(i);
    }
    sink = vec.size();
}

// Vector push_back with reserve
void bench_vec_push_reserved(int64_t iterations) {
    std::vector<int64_t> vec;
    vec.reserve(iterations);
    for (int64_t i = 0; i < iterations; ++i) {
        vec.push_back(i);
    }
    sink = vec.size();
}

// Vector random access
void bench_vec_access(int64_t iterations) {
    std::vector<int64_t> vec;
    vec.reserve(10000);
    for (int64_t i = 0; i < 10000; ++i) {
        vec.push_back(i * 2);
    }

    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += vec[i % 10000];
    }
    sink = sum;
}

// Vector iteration (sequential)
void bench_vec_iterate(int64_t iterations) {
    std::vector<int64_t> vec;
    vec.reserve(10000);
    for (int64_t i = 0; i < 10000; ++i) {
        vec.push_back(i);
    }

    int64_t sum = 0;
    for (int64_t round = 0; round < iterations / 10000; ++round) {
        for (int64_t i = 0; i < 10000; ++i) {
            sum += vec[i];
        }
    }
    sink = sum;
}

// Vector pop_back
void bench_vec_pop(int64_t iterations) {
    std::vector<int64_t> vec;
    vec.reserve(iterations);
    for (int64_t i = 0; i < iterations; ++i) {
        vec.push_back(i);
    }

    int64_t sum = 0;
    while (!vec.empty()) {
        sum += vec.back();
        vec.pop_back();
    }
    sink = sum;
}

// Vector set (modify elements)
void bench_vec_set(int64_t iterations) {
    std::vector<int64_t> vec(10000, 0);

    for (int64_t i = 0; i < iterations; ++i) {
        vec[i % 10000] = i;
    }
    sink = vec[0] + vec[9999];
}

int main() {
    bench::Benchmark b("List/Vec");

    const int64_t N = 10000000; // 10M ops for all benchmarks

    b.run_with_iter("List Push (grow)", N, bench_vec_push, 3);
    b.run_with_iter("List Push (reserved)", N, bench_vec_push_reserved, 3);
    b.run_with_iter("List Random Access", N, bench_vec_access, 3);
    b.run_with_iter("List Iteration", N, bench_vec_iterate, 3);
    b.run_with_iter("List Pop", N, bench_vec_pop, 3);
    b.run_with_iter("List Set", N, bench_vec_set, 3);

    b.print_results();
    b.save_json("../results/list_cpp.json");

    return 0;
}
