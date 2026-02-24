/**
 * HashMap Benchmarks (C++)
 *
 * Tests std::unordered_map operations: insert, lookup, contains, remove, string keys.
 * Matches TML hashmap_bench.tml and Rust hashmap_bench.rs for direct comparison.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

// Prevent optimization
volatile int64_t sink = 0;

// HashMap insert (I64 -> I64)
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
    bench::Benchmark b("HashMap");

    const int64_t MAP_ITER = 100000;
    const int64_t LOOKUP_ITER = 1000000;

    b.run_with_iter("HashMap Insert", MAP_ITER, bench_hashmap_insert, 10);
    b.run_with_iter("HashMap Insert (reserved)", MAP_ITER, bench_hashmap_insert_reserved, 10);
    b.run_with_iter("HashMap Lookup", LOOKUP_ITER, bench_hashmap_lookup, 10);
    b.run_with_iter("HashMap Contains", LOOKUP_ITER, bench_hashmap_contains, 10);
    b.run_with_iter("HashMap Remove", MAP_ITER, bench_hashmap_remove, 10);
    b.run_with_iter("HashMap String Key", MAP_ITER, bench_hashmap_string_key, 10);

    b.print_results();
    b.save_json("../results/hashmap_cpp.json");

    return 0;
}
