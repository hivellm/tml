/**
 * Collections Benchmarks (C++)
 *
 * Tests std::vector, std::unordered_map, and related operations.
 * Establishes baseline for TML collections comparison.
 */

#include "../common/bench.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

// Prevent optimization
volatile int64_t sink = 0;

// Vector push (grow from empty)
void bench_vec_push(int64_t iterations) {
    std::vector<int64_t> vec;
    for (int64_t i = 0; i < iterations; ++i) {
        vec.push_back(i);
    }
    sink = vec.size();
}

// Vector push with reserve
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
    std::vector<int64_t> vec(10000);
    for (int64_t i = 0; i < 10000; ++i) {
        vec[i] = i;
    }

    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        sum += vec[i % 10000];
    }
    sink = sum;
}

// Vector iteration
void bench_vec_iterate(int64_t iterations) {
    std::vector<int64_t> vec(10000);
    for (int64_t i = 0; i < 10000; ++i) {
        vec[i] = i;
    }

    int64_t sum = 0;
    for (int64_t round = 0; round < iterations / 10000; ++round) {
        for (const auto& v : vec) {
            sum += v;
        }
    }
    sink = sum;
}

// Vector pop
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

// Vector set (modify existing elements)
void bench_vec_set(int64_t iterations) {
    std::vector<int64_t> vec(10000, 0);

    for (int64_t i = 0; i < iterations; ++i) {
        vec[i % 10000] = i;
    }
    sink = vec[0];
}

// HashMap insert
void bench_hashmap_insert(int64_t iterations) {
    std::unordered_map<int64_t, int64_t> map;
    for (int64_t i = 0; i < iterations; ++i) {
        map[i] = i * 2;
    }
    sink = map.size();
}

// HashMap insert with reserve
void bench_hashmap_insert_reserved(int64_t iterations) {
    std::unordered_map<int64_t, int64_t> map;
    map.reserve(iterations);
    for (int64_t i = 0; i < iterations; ++i) {
        map[i] = i * 2;
    }
    sink = map.size();
}

// HashMap lookup
void bench_hashmap_lookup(int64_t iterations) {
    std::unordered_map<int64_t, int64_t> map;
    for (int64_t i = 0; i < 10000; ++i) {
        map[i] = i * 2;
    }

    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto it = map.find(i % 10000);
        if (it != map.end()) {
            sum += it->second;
        }
    }
    sink = sum;
}

// HashMap contains check
void bench_hashmap_contains(int64_t iterations) {
    std::unordered_map<int64_t, int64_t> map;
    for (int64_t i = 0; i < 10000; ++i) {
        map[i] = i;
    }

    int64_t found = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        // Check half existing, half non-existing
        if (map.count(i % 20000) > 0) {
            found++;
        }
    }
    sink = found;
}

// HashMap remove
void bench_hashmap_remove(int64_t iterations) {
    std::unordered_map<int64_t, int64_t> map;
    for (int64_t i = 0; i < iterations; ++i) {
        map[i] = i;
    }

    int64_t removed = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        removed += map.erase(i);
    }
    sink = removed;
}

// String key hashmap
void bench_hashmap_string_key(int64_t iterations) {
    std::unordered_map<std::string, int64_t> map;

    // Insert
    for (int64_t i = 0; i < iterations; ++i) {
        map["key" + std::to_string(i)] = i;
    }

    // Lookup
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        auto it = map.find("key" + std::to_string(i));
        if (it != map.end()) {
            sum += it->second;
        }
    }
    sink = sum;
}

int main() {
    bench::Benchmark b("Collections");

    const int64_t VEC_ITER = 1000000; // 1M for vector ops
    const int64_t MAP_ITER = 100000;  // 100K for map ops

    // Run benchmarks
    b.run_with_iter("Vec Push (grow)", VEC_ITER, bench_vec_push, 10);
    b.run_with_iter("Vec Push (reserved)", VEC_ITER, bench_vec_push_reserved, 10);
    b.run_with_iter("Vec Random Access", VEC_ITER, bench_vec_access, 10);
    b.run_with_iter("Vec Iteration", VEC_ITER, bench_vec_iterate, 10);
    b.run_with_iter("Vec Pop", VEC_ITER, bench_vec_pop, 10);
    b.run_with_iter("Vec Set", VEC_ITER, bench_vec_set, 10);
    b.run_with_iter("HashMap Insert", MAP_ITER, bench_hashmap_insert, 10);
    b.run_with_iter("HashMap Insert (reserved)", MAP_ITER, bench_hashmap_insert_reserved, 10);
    b.run_with_iter("HashMap Lookup", VEC_ITER, bench_hashmap_lookup, 10);
    b.run_with_iter("HashMap Contains", VEC_ITER, bench_hashmap_contains, 10);
    b.run_with_iter("HashMap Remove", MAP_ITER, bench_hashmap_remove, 10);
    b.run_with_iter("HashMap String Key", MAP_ITER, bench_hashmap_string_key, 10);

    // Print results
    b.print_results();

    // Save JSON
    b.save_json("../results/collections_cpp.json");

    return 0;
}
