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

    const int64_t N = 1000000; // 1M ops for all benchmarks

    b.run_with_iter("HashMap Insert", N, bench_hashmap_insert, 3);
    b.run_with_iter("HashMap Insert (reserved)", N, bench_hashmap_insert_reserved, 3);
    b.run_with_iter("HashMap Lookup", N, bench_hashmap_lookup, 3);
    b.run_with_iter("HashMap Contains", N, bench_hashmap_contains, 3);
    b.run_with_iter("HashMap Remove", N, bench_hashmap_remove, 3);
    b.run_with_iter("HashMap String Key", N, bench_hashmap_string_key, 3);

    b.print_results();
    b.save_json("../results/hashmap_cpp.json");

    return 0;
}
