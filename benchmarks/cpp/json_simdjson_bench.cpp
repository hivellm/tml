//! # JSON Benchmark - TML vs simdjson Comparison
//!
//! Compares TML's V8-optimized JSON parser against simdjson.
//!
//! ## Requirements
//!
//! Download simdjson single-header from:
//! https://github.com/simdjson/simdjson/releases/latest
//!
//! Place `simdjson.h` and `simdjson.cpp` in this directory.
//!
//! ## Build
//!
//! ```bash
//! # Windows (MSVC)
//! cl /O2 /EHsc /std:c++20 /I../../compiler/include json_simdjson_bench.cpp ^
//!    simdjson.cpp ../../compiler/src/json/*.cpp /Fe:json_simdjson_bench.exe
//!
//! # Linux/macOS (clang++)
//! clang++ -O3 -std=c++20 -msse4.2 -mpclmul -I../../compiler/include \
//!     json_simdjson_bench.cpp simdjson.cpp ../../compiler/src/json/*.cpp \
//!     -o json_simdjson_bench
//! ```
//!
//! ## Results Interpretation
//!
//! simdjson achieves its speed through:
//! - Full SIMD parsing (AVX2/SSE4.2)
//! - On-demand (lazy) parsing - doesn't materialize all values
//! - Zero-copy string access
//!
//! TML's fast parser goals:
//! - Good performance without external dependencies
//! - Full materialization (all values parsed immediately)
//! - Standard std::string ownership
//!
//! Expected result: simdjson ~2-5x faster on large inputs due to lazy parsing
//! and more extensive SIMD usage.

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// TML JSON
#include "json/json.hpp"

// simdjson (if available)
#if __has_include("simdjson.h")
#include "simdjson.h"
#define HAS_SIMDJSON 1
#else
#define HAS_SIMDJSON 0
#endif

using namespace std::chrono;

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

struct BenchResult {
    std::string name;
    double time_us;
    size_t iterations;
    double throughput_mb_s;
    bool success;
};

template <typename Func>
auto benchmark(const std::string& name, size_t iterations, size_t data_size, Func&& func)
    -> BenchResult {
    // Warmup
    bool success = true;
    for (size_t i = 0; i < std::min(iterations / 10, size_t(10)); ++i) {
        if (!func()) {
            success = false;
            break;
        }
    }

    if (!success) {
        return {name, 0, iterations, 0, false};
    }

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        func();
    }
    auto end = high_resolution_clock::now();

    double total_us = duration_cast<microseconds>(end - start).count();
    double avg_us = total_us / iterations;
    double throughput = (data_size * iterations) / (total_us / 1e6) / (1024 * 1024);

    return {name, avg_us, iterations, throughput, true};
}

void print_result(const BenchResult& r) {
    if (!r.success) {
        std::cout << std::left << std::setw(45) << r.name << " FAILED\n";
        return;
    }
    std::cout << std::left << std::setw(45) << r.name << std::right << std::setw(10) << std::fixed
              << std::setprecision(2) << r.time_us << " us" << std::setw(10) << std::fixed
              << std::setprecision(2) << r.throughput_mb_s << " MB/s\n";
}

void print_comparison(const BenchResult& tml, const BenchResult& simdjson) {
    if (!tml.success || !simdjson.success)
        return;
    double speedup = tml.time_us / simdjson.time_us;
    std::cout << "  -> simdjson speedup: " << std::fixed << std::setprecision(2) << speedup << "x";
    if (speedup < 1.0) {
        std::cout << " (TML faster!)";
    }
    std::cout << "\n";
}

void print_separator() {
    std::cout << std::string(80, '-') << "\n";
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
        if (i > 0)
            ss << ",";
        ss << R"({"id":)" << i << R"(,"name":"Item )" << i << R"(","price":)" << (i * 1.5)
           << R"(,"active":)" << (i % 2 == 0 ? "true" : "false")
           << R"(,"tags":["tag1","tag2","tag3"]})";
    }
    ss << "]}";
    return ss.str();
}

std::string generate_large_json(size_t num_items = 10000) {
    std::ostringstream ss;
    ss << "{\"data\": [";
    for (size_t i = 0; i < num_items; ++i) {
        if (i > 0)
            ss << ",";
        ss << R"({"id":)" << i << R"(,"uuid":"550e8400-e29b-41d4-a716-446655440)"
           << std::setfill('0') << std::setw(3) << (i % 1000) << R"(","name":"User )" << i
           << R"(","email":"user)" << i << R"(@example.com")"
           << R"(,"score":)" << (i * 0.1)
           << R"(,"metadata":{"created":"2024-01-01","updated":"2024-01-02","version":)" << (i % 10)
           << R"(},"tags":["alpha","beta","gamma","delta"]})";
    }
    ss << "]}";
    return ss.str();
}

std::string generate_wide_array(size_t size = 10000) {
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < size; ++i) {
        if (i > 0)
            ss << ",";
        ss << i;
    }
    ss << "]";
    return ss.str();
}

std::string generate_string_heavy_json(size_t num_items = 1000) {
    std::ostringstream ss;
    ss << "{\"strings\": [";
    for (size_t i = 0; i < num_items; ++i) {
        if (i > 0)
            ss << ",";
        ss << "\"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor "
              "incididunt ut labore et dolore magna aliqua. Item "
           << i << "\"";
    }
    ss << "]}";
    return ss.str();
}

// ============================================================================
// Benchmark Functions
// ============================================================================

#if HAS_SIMDJSON
void run_comparison_benchmarks() {
    std::cout << "\n=== TML vs simdjson Comparison ===\n\n";

    std::cout << "Parser implementations:\n";
    std::cout << "  TML Fast: V8-inspired, SIMD whitespace/strings, full materialization\n";
    std::cout << "  simdjson: Full SIMD parsing, on-demand (lazy) value access\n\n";

    print_separator();

    std::vector<std::pair<BenchResult, BenchResult>> comparisons;

    // Small JSON
    {
        auto json = generate_small_json();
        // Pad JSON for simdjson (requires SIMDJSON_PADDING bytes at end)
        std::string padded_json = json + std::string(simdjson::SIMDJSON_PADDING, '\0');

        std::cout << "Small JSON (" << json.size() << " bytes):\n";

        simdjson::dom::parser simdjson_parser;
        auto r_simd = benchmark("  simdjson (DOM)", 100000, json.size(), [&]() {
            auto result = simdjson_parser.parse(padded_json.data(), json.size());
            return !result.error();
        });
        print_result(r_simd);

        auto r_tml = benchmark("  TML Fast", 100000, json.size(), [&]() {
            auto result = tml::json::fast::parse_json_fast(json);
            return tml::is_ok(result);
        });
        print_result(r_tml);

        print_comparison(r_tml, r_simd);
        comparisons.push_back({r_tml, r_simd});
        std::cout << "\n";
    }

    // Medium JSON
    {
        auto json = generate_medium_json(1000);
        std::string padded_json = json + std::string(simdjson::SIMDJSON_PADDING, '\0');

        std::cout << "Medium JSON (" << json.size() / 1024 << " KB):\n";

        simdjson::dom::parser simdjson_parser;
        auto r_simd = benchmark("  simdjson (DOM)", 1000, json.size(), [&]() {
            auto result = simdjson_parser.parse(padded_json.data(), json.size());
            return !result.error();
        });
        print_result(r_simd);

        auto r_tml = benchmark("  TML Fast", 1000, json.size(), [&]() {
            auto result = tml::json::fast::parse_json_fast(json);
            return tml::is_ok(result);
        });
        print_result(r_tml);

        print_comparison(r_tml, r_simd);
        comparisons.push_back({r_tml, r_simd});
        std::cout << "\n";
    }

    // Large JSON
    {
        auto json = generate_large_json(10000);
        std::string padded_json = json + std::string(simdjson::SIMDJSON_PADDING, '\0');

        std::cout << "Large JSON (" << json.size() / 1024 << " KB):\n";

        simdjson::dom::parser simdjson_parser;
        auto r_simd = benchmark("  simdjson (DOM)", 100, json.size(), [&]() {
            auto result = simdjson_parser.parse(padded_json.data(), json.size());
            return !result.error();
        });
        print_result(r_simd);

        auto r_tml = benchmark("  TML Fast", 100, json.size(), [&]() {
            auto result = tml::json::fast::parse_json_fast(json);
            return tml::is_ok(result);
        });
        print_result(r_tml);

        print_comparison(r_tml, r_simd);
        comparisons.push_back({r_tml, r_simd});
        std::cout << "\n";
    }

    // Wide array (integer parsing)
    {
        auto json = generate_wide_array(10000);
        std::string padded_json = json + std::string(simdjson::SIMDJSON_PADDING, '\0');

        std::cout << "Wide array of integers (" << json.size() / 1024 << " KB):\n";

        simdjson::dom::parser simdjson_parser;
        auto r_simd = benchmark("  simdjson (DOM)", 1000, json.size(), [&]() {
            auto result = simdjson_parser.parse(padded_json.data(), json.size());
            return !result.error();
        });
        print_result(r_simd);

        auto r_tml = benchmark("  TML Fast", 1000, json.size(), [&]() {
            auto result = tml::json::fast::parse_json_fast(json);
            return tml::is_ok(result);
        });
        print_result(r_tml);

        print_comparison(r_tml, r_simd);
        comparisons.push_back({r_tml, r_simd});
        std::cout << "\n";
    }

    // String-heavy JSON
    {
        auto json = generate_string_heavy_json(1000);
        std::string padded_json = json + std::string(simdjson::SIMDJSON_PADDING, '\0');

        std::cout << "String-heavy JSON (" << json.size() / 1024 << " KB):\n";

        simdjson::dom::parser simdjson_parser;
        auto r_simd = benchmark("  simdjson (DOM)", 500, json.size(), [&]() {
            auto result = simdjson_parser.parse(padded_json.data(), json.size());
            return !result.error();
        });
        print_result(r_simd);

        auto r_tml = benchmark("  TML Fast", 500, json.size(), [&]() {
            auto result = tml::json::fast::parse_json_fast(json);
            return tml::is_ok(result);
        });
        print_result(r_tml);

        print_comparison(r_tml, r_simd);
        comparisons.push_back({r_tml, r_simd});
        std::cout << "\n";
    }

    print_separator();

    // Summary
    std::cout << "\n=== Summary ===\n\n";

    double total_tml_time = 0;
    double total_simd_time = 0;
    for (const auto& [tml_r, simd_r] : comparisons) {
        if (tml_r.success)
            total_tml_time += tml_r.time_us;
        if (simd_r.success)
            total_simd_time += simd_r.time_us;
    }

    std::cout << "Total TML Fast time:   " << std::fixed << std::setprecision(2)
              << total_tml_time / 1000 << " ms\n";
    std::cout << "Total simdjson time:   " << std::fixed << std::setprecision(2)
              << total_simd_time / 1000 << " ms\n";
    std::cout << "simdjson speedup:      " << std::fixed << std::setprecision(2)
              << total_tml_time / total_simd_time << "x\n";

    std::cout << "\n";
    print_separator();
    std::cout << "\nNotes:\n";
    std::cout << "- simdjson uses lazy (on-demand) parsing - values are only materialized when "
                 "accessed\n";
    std::cout << "- TML fully materializes all values during parse (like Python json, JavaScript "
                 "JSON.parse)\n";
    std::cout << "- For fair comparison of full parsing, both would need to iterate all values\n";
    std::cout << "- TML achieves good performance without external SIMD dependencies\n";
}

#else

void run_comparison_benchmarks() {
    std::cout << "\n=== simdjson not available ===\n\n";
    std::cout << "To run simdjson comparison:\n";
    std::cout << "1. Download from: https://github.com/simdjson/simdjson/releases\n";
    std::cout << "2. Place simdjson.h and simdjson.cpp in this directory\n";
    std::cout << "3. Rebuild with simdjson sources\n\n";

    print_separator();
    std::cout << "\nRunning TML-only benchmarks:\n\n";

    // Run TML-only benchmarks
    auto small = generate_small_json();
    auto r1 = benchmark("TML Fast: Small JSON", 100000, small.size(), [&]() {
        auto result = tml::json::fast::parse_json_fast(small);
        return tml::is_ok(result);
    });
    print_result(r1);

    auto medium = generate_medium_json(1000);
    auto r2 = benchmark("TML Fast: Medium JSON", 1000, medium.size(), [&]() {
        auto result = tml::json::fast::parse_json_fast(medium);
        return tml::is_ok(result);
    });
    print_result(r2);

    auto large = generate_large_json(10000);
    auto r3 = benchmark("TML Fast: Large JSON", 100, large.size(), [&]() {
        auto result = tml::json::fast::parse_json_fast(large);
        return tml::is_ok(result);
    });
    print_result(r3);

    auto wide = generate_wide_array(10000);
    auto r4 = benchmark("TML Fast: Wide array", 1000, wide.size(), [&]() {
        auto result = tml::json::fast::parse_json_fast(wide);
        return tml::is_ok(result);
    });
    print_result(r4);

    auto strings = generate_string_heavy_json(1000);
    auto r5 = benchmark("TML Fast: String-heavy", 500, strings.size(), [&]() {
        auto result = tml::json::fast::parse_json_fast(strings);
        return tml::is_ok(result);
    });
    print_result(r5);

    print_separator();

    std::cout << "\nExpected simdjson comparison results (based on typical benchmarks):\n";
    std::cout << "  - Small JSON: simdjson ~1.5-2x faster\n";
    std::cout << "  - Large JSON: simdjson ~2-4x faster (SIMD shines on big data)\n";
    std::cout << "  - String-heavy: simdjson ~2-3x faster (SIMD string scanning)\n";
    std::cout << "\nTML advantages:\n";
    std::cout << "  - No external dependencies\n";
    std::cout << "  - Simpler integration\n";
    std::cout << "  - Full value materialization (no lazy evaluation surprises)\n";
    std::cout << "  - Standard std::string ownership model\n";
}

#endif

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "JSON Benchmark: TML vs simdjson\n";
    std::cout << "===============================\n";

#if HAS_SIMDJSON
    std::cout << "simdjson: AVAILABLE\n";
#else
    std::cout << "simdjson: NOT AVAILABLE (see instructions below)\n";
#endif

    std::cout << "\nTest data sizes:\n";
    std::cout << "  Small JSON:  " << generate_small_json().size() << " bytes\n";
    std::cout << "  Medium JSON: " << generate_medium_json(1000).size() / 1024 << " KB\n";
    std::cout << "  Large JSON:  " << generate_large_json(10000).size() / 1024 << " KB\n";
    std::cout << "  Wide Array:  " << generate_wide_array(10000).size() / 1024 << " KB\n";
    std::cout << "  String-heavy:" << generate_string_heavy_json(1000).size() / 1024 << " KB\n";

    run_comparison_benchmarks();

    std::cout << "\nBenchmark complete.\n";
    return 0;
}
