//! # JSON Benchmark - TML Parser Comparison
//!
//! Compares TML's original JSON parser vs the V8-optimized fast parser.
//!
//! ## Benchmarks
//!
//! 1. Parse small JSON (< 1KB)
//! 2. Parse medium JSON (~100KB)
//! 3. Parse large JSON (~1MB)
//! 4. Serialize to string
//! 5. Deep nesting performance
//! 6. Large array performance
//!
//! ## Build
//!
//! ```bash
//! clang++ -O3 -std=c++20 -msse2 -I../../compiler/include -I../../compiler/src json_bench.cpp \
//!     ../../compiler/src/json/*.cpp -o json_bench
//! ```

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// TML JSON (includes both original and fast parser)
#include "json/json.hpp"

using namespace std::chrono;

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

struct BenchResult {
    std::string name;
    double time_us;
    size_t iterations;
    double throughput_mb_s;
};

template <typename Func>
auto benchmark(const std::string& name, size_t iterations, size_t data_size, Func&& func)
    -> BenchResult {
    // Warmup
    for (size_t i = 0; i < std::min(iterations / 10, size_t(10)); ++i) {
        func();
    }

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        func();
    }
    auto end = high_resolution_clock::now();

    double total_us = duration_cast<microseconds>(end - start).count();
    double avg_us = total_us / iterations;
    double throughput = (data_size * iterations) / (total_us / 1e6) / (1024 * 1024);

    return {name, avg_us, iterations, throughput};
}

void print_result(const BenchResult& r) {
    std::cout << std::left << std::setw(45) << r.name << std::right << std::setw(12)
              << std::fixed << std::setprecision(2) << r.time_us << " us" << std::setw(12)
              << r.iterations << " iters";
    if (r.throughput_mb_s > 0) {
        std::cout << std::setw(12) << std::fixed << std::setprecision(2) << r.throughput_mb_s
                  << " MB/s";
    }
    std::cout << "\n";
}

void print_comparison(const BenchResult& original, const BenchResult& fast) {
    double speedup = original.time_us / fast.time_us;
    std::cout << "  -> Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n";
}

void print_separator() {
    std::cout << std::string(90, '-') << "\n";
}

// ============================================================================
// Test Data Generation
// ============================================================================

std::string generate_small_json() {
    return R"({
        "name": "John Doe",
        "age": 30,
        "active": true,
        "email": "john@example.com",
        "scores": [95, 87, 92, 88, 91],
        "address": {
            "street": "123 Main St",
            "city": "New York",
            "zip": "10001"
        }
    })";
}

std::string generate_medium_json(size_t num_items = 1000) {
    std::ostringstream ss;
    ss << "{\"items\": [";
    for (size_t i = 0; i < num_items; ++i) {
        if (i > 0) ss << ",";
        ss << R"({"id":)" << i << R"(,"name":"Item )" << i
           << R"(","price":)" << (i * 1.5) << R"(,"active":)" << (i % 2 == 0 ? "true" : "false")
           << R"(,"tags":["tag1","tag2","tag3"]})";
    }
    ss << "]}";
    return ss.str();
}

std::string generate_large_json(size_t num_items = 10000) {
    std::ostringstream ss;
    ss << "{\"data\": [";
    for (size_t i = 0; i < num_items; ++i) {
        if (i > 0) ss << ",";
        ss << R"({"id":)" << i << R"(,"uuid":"550e8400-e29b-41d4-a716-446655440)" << std::setfill('0')
           << std::setw(3) << (i % 1000)
           << R"(","name":"User )" << i
           << R"(","email":"user)" << i << R"(@example.com")"
           << R"(,"score":)" << (i * 0.1)
           << R"(,"metadata":{"created":"2024-01-01","updated":"2024-01-02","version":)" << (i % 10)
           << R"(},"tags":["alpha","beta","gamma","delta"]})";
    }
    ss << "]}";
    return ss.str();
}

std::string generate_deep_json(size_t depth = 100) {
    std::string json;
    for (size_t i = 0; i < depth; ++i) {
        json += R"({"level":)" + std::to_string(i) + R"(,"child":)";
    }
    json += "null";
    for (size_t i = 0; i < depth; ++i) {
        json += "}";
    }
    return json;
}

std::string generate_wide_array(size_t size = 10000) {
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < size; ++i) {
        if (i > 0) ss << ",";
        ss << i;
    }
    ss << "]";
    return ss.str();
}

std::string generate_string_heavy_json(size_t num_items = 1000) {
    std::ostringstream ss;
    ss << "{\"strings\": [";
    for (size_t i = 0; i < num_items; ++i) {
        if (i > 0) ss << ",";
        ss << "\"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor "
              "incididunt ut labore et dolore magna aliqua. Item "
           << i << "\"";
    }
    ss << "]}";
    return ss.str();
}

// ============================================================================
// Comparison Benchmarks
// ============================================================================

void run_comparison_benchmarks() {
    std::cout << "\n=== TML JSON Parser Comparison: Original vs Fast (V8-optimized) ===\n\n";
    print_separator();

    std::vector<std::pair<BenchResult, BenchResult>> comparisons;

    // Small JSON parsing
    {
        auto json = generate_small_json();
        std::cout << "Small JSON (" << json.size() << " bytes):\n";

        auto r_orig = benchmark(
            "  Original parser", 100000, json.size(),
            [&]() { (void)tml::json::parse_json(json); });
        print_result(r_orig);

        auto r_fast = benchmark(
            "  Fast parser (SIMD)", 100000, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        print_result(r_fast);

        print_comparison(r_orig, r_fast);
        comparisons.push_back({r_orig, r_fast});
        std::cout << "\n";
    }

    // Medium JSON parsing
    {
        auto json = generate_medium_json(1000);
        std::cout << "Medium JSON (" << json.size() << " bytes):\n";

        auto r_orig = benchmark(
            "  Original parser", 1000, json.size(),
            [&]() { (void)tml::json::parse_json(json); });
        print_result(r_orig);

        auto r_fast = benchmark(
            "  Fast parser (SIMD)", 1000, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        print_result(r_fast);

        print_comparison(r_orig, r_fast);
        comparisons.push_back({r_orig, r_fast});
        std::cout << "\n";
    }

    // Large JSON parsing
    {
        auto json = generate_large_json(10000);
        std::cout << "Large JSON (" << json.size() << " bytes):\n";

        auto r_orig = benchmark(
            "  Original parser", 100, json.size(),
            [&]() { (void)tml::json::parse_json(json); });
        print_result(r_orig);

        auto r_fast = benchmark(
            "  Fast parser (SIMD)", 100, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        print_result(r_fast);

        print_comparison(r_orig, r_fast);
        comparisons.push_back({r_orig, r_fast});
        std::cout << "\n";
    }

    // Deep nesting
    {
        auto json = generate_deep_json(100);
        std::cout << "Deep nesting (100 levels, " << json.size() << " bytes):\n";

        auto r_orig = benchmark(
            "  Original parser", 10000, json.size(),
            [&]() { (void)tml::json::parse_json(json); });
        print_result(r_orig);

        auto r_fast = benchmark(
            "  Fast parser (SIMD)", 10000, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        print_result(r_fast);

        print_comparison(r_orig, r_fast);
        comparisons.push_back({r_orig, r_fast});
        std::cout << "\n";
    }

    // Wide array
    {
        auto json = generate_wide_array(10000);
        std::cout << "Wide array (10K ints, " << json.size() << " bytes):\n";

        auto r_orig = benchmark(
            "  Original parser", 1000, json.size(),
            [&]() { (void)tml::json::parse_json(json); });
        print_result(r_orig);

        auto r_fast = benchmark(
            "  Fast parser (SIMD)", 1000, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        print_result(r_fast);

        print_comparison(r_orig, r_fast);
        comparisons.push_back({r_orig, r_fast});
        std::cout << "\n";
    }

    // String-heavy JSON
    {
        auto json = generate_string_heavy_json(1000);
        std::cout << "String-heavy JSON (" << json.size() << " bytes):\n";

        auto r_orig = benchmark(
            "  Original parser", 500, json.size(),
            [&]() { (void)tml::json::parse_json(json); });
        print_result(r_orig);

        auto r_fast = benchmark(
            "  Fast parser (SIMD)", 500, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        print_result(r_fast);

        print_comparison(r_orig, r_fast);
        comparisons.push_back({r_orig, r_fast});
        std::cout << "\n";
    }

    print_separator();

    // Summary
    std::cout << "\n=== Summary ===\n\n";

    double total_orig_time = 0;
    double total_fast_time = 0;
    for (const auto& [orig, fast] : comparisons) {
        total_orig_time += orig.time_us;
        total_fast_time += fast.time_us;
    }

    std::cout << "Total original parser time: " << std::fixed << std::setprecision(2)
              << total_orig_time / 1000 << " ms\n";
    std::cout << "Total fast parser time:     " << std::fixed << std::setprecision(2)
              << total_fast_time / 1000 << " ms\n";
    std::cout << "Overall speedup:            " << std::fixed << std::setprecision(2)
              << total_orig_time / total_fast_time << "x\n";

    std::cout << "\n";
    print_separator();
    std::cout << "\nOptimizations in fast parser:\n";
    std::cout << "  - O(1) character lookup tables (like V8)\n";
    std::cout << "  - SIMD whitespace skipping (SSE2)\n";
    std::cout << "  - SIMD string scanning for quotes/escapes\n";
    std::cout << "  - SWAR hex digit parsing for \\uXXXX\n";
    std::cout << "  - Single-pass parsing (no separate lexer)\n";
    std::cout << "  - SMI fast path for small integers\n";
    std::cout << "  - Pre-allocated string buffers\n";
}

// ============================================================================
// Original benchmarks (for comparison with other languages)
// ============================================================================

void run_fast_parser_benchmarks() {
    std::cout << "\n=== TML Fast JSON Parser (for external comparison) ===\n\n";
    print_separator();

    std::vector<BenchResult> results;

    // Small JSON parsing
    {
        auto json = generate_small_json();
        auto r = benchmark(
            "TML Fast: Parse small JSON", 100000, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        results.push_back(r);
        print_result(r);
    }

    // Medium JSON parsing
    {
        auto json = generate_medium_json(1000);
        auto r = benchmark(
            "TML Fast: Parse medium JSON (100KB)", 1000, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        results.push_back(r);
        print_result(r);
    }

    // Large JSON parsing
    {
        auto json = generate_large_json(10000);
        auto r = benchmark(
            "TML Fast: Parse large JSON (1MB)", 100, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        results.push_back(r);
        print_result(r);
    }

    // Deep nesting
    {
        auto json = generate_deep_json(100);
        auto r = benchmark(
            "TML Fast: Parse deep nesting (100 levels)", 10000, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        results.push_back(r);
        print_result(r);
    }

    // Wide array
    {
        auto json = generate_wide_array(10000);
        auto r = benchmark(
            "TML Fast: Parse wide array (10K ints)", 1000, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        results.push_back(r);
        print_result(r);
    }

    // String-heavy JSON
    {
        auto json = generate_string_heavy_json(1000);
        auto r = benchmark(
            "TML Fast: Parse string-heavy JSON", 500, json.size(),
            [&]() { (void)tml::json::fast::parse_json_fast(json); });
        results.push_back(r);
        print_result(r);
    }

    print_separator();

    // Summary
    std::cout << "\n=== Summary ===\n\n";
    double total_time = 0;
    for (const auto& r : results) {
        total_time += r.time_us;
    }
    std::cout << "Total benchmark time: " << std::fixed << std::setprecision(2) << total_time / 1000
              << " ms\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "JSON Benchmark Suite - TML Native Implementation\n";
    std::cout << "================================================\n";

    // Show test data sizes
    std::cout << "\nTest data sizes:\n";
    std::cout << "  Small JSON:  " << generate_small_json().size() << " bytes\n";
    std::cout << "  Medium JSON: " << generate_medium_json(1000).size() << " bytes\n";
    std::cout << "  Large JSON:  " << generate_large_json(10000).size() << " bytes\n";
    std::cout << "  Deep JSON:   " << generate_deep_json(100).size() << " bytes\n";
    std::cout << "  Wide Array:  " << generate_wide_array(10000).size() << " bytes\n";
    std::cout << "  String-heavy:" << generate_string_heavy_json(1000).size() << " bytes\n";

    // Run comparison benchmarks
    run_comparison_benchmarks();

    // Run fast parser benchmarks for external comparison
    run_fast_parser_benchmarks();

    std::cout << "\nBenchmark complete.\n";
    return 0;
}
