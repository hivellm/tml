/**
 * String Benchmarks (C++)
 *
 * Tests string operations performance.
 * Establishes baseline for TML string comparison.
 */

#include "../common/bench.hpp"

#include <cstdint>
#include <cstring>

// Prevent optimization
volatile int64_t sink = 0;
volatile const char* str_sink = nullptr;

// String concatenation (small strings)
void bench_concat_small(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        std::string result = "Hello";
        result += " ";
        result += "World";
        result += "!";
        bench::do_not_optimize(result.data());
    }
}

// String concatenation (building a longer string)
void bench_concat_loop(int64_t iterations) {
    std::string result;
    result.reserve(iterations * 2); // Pre-allocate like TML Text would
    for (int64_t i = 0; i < iterations; ++i) {
        result += "ab";
    }
    sink = result.size();
}

// String concatenation without reserve (O(n^2) pattern like basic TML Str)
void bench_concat_naive(int64_t iterations) {
    std::string result;
    for (int64_t i = 0; i < iterations; ++i) {
        result += "ab";
    }
    sink = result.size();
}

// String length
void bench_strlen(int64_t iterations) {
    const char* str = "The quick brown fox jumps over the lazy dog";
    int64_t total = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        total += strlen(str);
    }
    sink = total;
}

// String comparison
void bench_strcmp_equal(int64_t iterations) {
    const char* s1 = "Hello, World!";
    const char* s2 = "Hello, World!";
    int64_t matches = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        if (strcmp(s1, s2) == 0) {
            matches++;
        }
    }
    sink = matches;
}

void bench_strcmp_different(int64_t iterations) {
    const char* s1 = "Hello, World!";
    const char* s2 = "Hello, World?";
    int64_t matches = 0;
    for (int64_t i = 0; i < iterations; ++i) {
        if (strcmp(s1, s2) == 0) {
            matches++;
        }
    }
    sink = matches;
}

// Integer to string conversion
void bench_int_to_str(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        std::string s = std::to_string(i);
        bench::do_not_optimize(s.data());
    }
}

// String copy
void bench_strcpy(int64_t iterations) {
    const char* src = "The quick brown fox jumps over the lazy dog";
    char dst[64];
    for (int64_t i = 0; i < iterations; ++i) {
        strcpy(dst, src);
        bench::do_not_optimize(dst);
    }
}

// String from char repeated
void bench_string_repeat(int64_t iterations) {
    for (int64_t i = 0; i < iterations; ++i) {
        std::string s(50, 'x');
        bench::do_not_optimize(s.data());
    }
}

// Building string with sprintf
void bench_sprintf(int64_t iterations) {
    char buffer[128];
    for (int64_t i = 0; i < iterations; ++i) {
        snprintf(buffer, sizeof(buffer), "Value: %lld, Name: %s", i, "test");
        bench::do_not_optimize(buffer);
    }
}

// String append in loop (simulates log building)
void bench_log_building(int64_t iterations) {
    std::string log;
    log.reserve(iterations * 32);
    for (int64_t i = 0; i < iterations; ++i) {
        log += "[INFO] Message number ";
        log += std::to_string(i);
        log += "\n";
    }
    sink = log.size();
}

int main() {
    bench::Benchmark b("String");

    const int64_t ITERATIONS = 1000000; // 1M for fast ops
    const int64_t CONCAT_ITER = 100000; // 100K for concat
    const int64_t LOG_ITER = 10000;     // 10K for log building

    // Run benchmarks
    b.run_with_iter("Concat Small (3 strings)", ITERATIONS, bench_concat_small, 100);
    b.run_with_iter("Concat Loop (with reserve)", CONCAT_ITER, bench_concat_loop, 10,
                    "O(n) amortized");
    b.run_with_iter("Concat Loop (naive)", CONCAT_ITER, bench_concat_naive, 10,
                    "O(n^2) worst case");
    b.run_with_iter("String Length", ITERATIONS, bench_strlen, 100);
    b.run_with_iter("String Compare (equal)", ITERATIONS, bench_strcmp_equal, 100);
    b.run_with_iter("String Compare (different)", ITERATIONS, bench_strcmp_different, 100);
    b.run_with_iter("Int to String", ITERATIONS, bench_int_to_str, 100);
    b.run_with_iter("String Copy", ITERATIONS, bench_strcpy, 100);
    b.run_with_iter("String Repeat (50 chars)", ITERATIONS, bench_string_repeat, 100);
    b.run_with_iter("Sprintf Formatting", ITERATIONS, bench_sprintf, 100);
    b.run_with_iter("Log Building", LOG_ITER, bench_log_building, 10);

    // Print results
    b.print_results();

    // Save JSON
    b.save_json("../results/string_cpp.json");

    return 0;
}
