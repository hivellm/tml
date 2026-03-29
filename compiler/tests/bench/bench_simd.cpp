//! # SIMD Benchmark Harness
//!
//! Timing framework with warm-up, statistical reporting (min, median, p95, stddev).
//! Uses QueryPerformanceCounter on Windows, clock_gettime on Linux/macOS.
//!
//! Usage:
//!   bench("dot_product_f32_avx2", 10000, [&]() {
//!       volatile float r = dot_product_f32(a, b, dim);
//!   });
//!
//! Build:
//!   scripts\build.bat --bench

#include "simd/simd_charclass.h"
#include "simd/simd_detect.hpp"
#include "simd/simd_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// ============================================================================
// High-resolution timer
// ============================================================================

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace bench {

static auto timer_freq() -> double {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return static_cast<double>(freq.QuadPart);
}

static auto now_ns() -> int64_t {
    static const double freq = timer_freq();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<int64_t>(static_cast<double>(counter.QuadPart) / freq * 1e9);
}

} // namespace bench

#else
#include <time.h>

namespace bench {

static auto now_ns() -> int64_t {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

} // namespace bench
#endif

// ============================================================================
// Statistical helpers
// ============================================================================

namespace bench {

struct Stats {
    double min_ns;
    double median_ns;
    double mean_ns;
    double p95_ns;
    double max_ns;
    double stddev_ns;
    size_t iterations;
};

static auto compute_stats(std::vector<int64_t>& samples) -> Stats {
    Stats s{};
    s.iterations = samples.size();

    if (samples.empty())
        return s;

    std::sort(samples.begin(), samples.end());

    s.min_ns = static_cast<double>(samples.front());
    s.max_ns = static_cast<double>(samples.back());

    // Median
    size_t n = samples.size();
    if (n % 2 == 0) {
        s.median_ns = static_cast<double>(samples[n / 2 - 1] + samples[n / 2]) / 2.0;
    } else {
        s.median_ns = static_cast<double>(samples[n / 2]);
    }

    // Mean
    double sum = 0;
    for (auto v : samples)
        sum += static_cast<double>(v);
    s.mean_ns = sum / static_cast<double>(n);

    // Stddev
    double var_sum = 0;
    for (auto v : samples) {
        double d = static_cast<double>(v) - s.mean_ns;
        var_sum += d * d;
    }
    s.stddev_ns = std::sqrt(var_sum / static_cast<double>(n));

    // P95
    size_t p95_idx = static_cast<size_t>(static_cast<double>(n) * 0.95);
    if (p95_idx >= n)
        p95_idx = n - 1;
    s.p95_ns = static_cast<double>(samples[p95_idx]);

    return s;
}

// ============================================================================
// Benchmark runner
// ============================================================================

struct BenchResult {
    std::string name;
    Stats stats;
};

static std::vector<BenchResult> g_results;

// Run `func` for `iterations` with 10% warm-up, collect per-iteration timings.
template <typename Func> void run_bench(const char* name, size_t iterations, Func func) {
    // Warm-up: 10% of iterations (minimum 10)
    size_t warmup = std::max<size_t>(iterations / 10, 10);
    for (size_t i = 0; i < warmup; ++i) {
        func();
    }

    // Timed iterations
    std::vector<int64_t> samples;
    samples.reserve(iterations);

    for (size_t i = 0; i < iterations; ++i) {
        auto t0 = now_ns();
        func();
        auto t1 = now_ns();
        samples.push_back(t1 - t0);
    }

    auto stats = compute_stats(samples);

    // Print immediately
    std::printf("  %-40s  min=%8.1f  median=%8.1f  p95=%8.1f  stddev=%7.1f ns  (%zu iters)\n", name,
                stats.min_ns, stats.median_ns, stats.p95_ns, stats.stddev_ns, iterations);

    g_results.push_back({name, stats});
}

// Print summary table at end
static void print_summary() {
    std::printf("\n=== Benchmark Summary ===\n");
    std::printf("%-42s  %10s  %10s  %10s  %10s\n", "Name", "Min(ns)", "Median(ns)", "P95(ns)",
                "Stddev(ns)");
    std::printf("%-42s  %10s  %10s  %10s  %10s\n", "------------------------------------------",
                "----------", "----------", "----------", "----------");
    for (auto& r : g_results) {
        std::printf("%-42s  %10.1f  %10.1f  %10.1f  %10.1f\n", r.name.c_str(), r.stats.min_ns,
                    r.stats.median_ns, r.stats.p95_ns, r.stats.stddev_ns);
    }
}

// Print detected SIMD features
static void print_features() {
    std::printf("=== SIMD Feature Detection ===\n");
    std::printf("  SSE2:    %s\n", tml::simd::has_sse2() ? "YES" : "NO");
    std::printf("  SSE4.2:  %s\n", tml::simd::has_sse42() ? "YES" : "NO");
    std::printf("  AVX2:    %s\n", tml::simd::has_avx2() ? "YES" : "NO");
    std::printf("  AVX-512: %s\n", tml::simd::has_avx512() ? "YES" : "NO");
    std::printf("  AES-NI:  %s\n", tml::simd::has_aesni() ? "YES" : "NO");
    std::printf("  POPCNT:  %s\n", tml::simd::has_popcnt() ? "YES" : "NO");
    std::printf("  FMA:     %s\n", tml::simd::has_fma() ? "YES" : "NO");
    std::printf("  BMI1:    %s\n", tml::simd::has_bmi1() ? "YES" : "NO");
    std::printf("  BMI2:    %s\n", tml::simd::has_bmi2() ? "YES" : "NO");
    std::printf("\n");
}

} // namespace bench

// ============================================================================
// Charclass table validation benchmarks
// ============================================================================

static void bench_charclass_tables() {
    std::printf("=== Character Classification ===\n");

    // Validate tables are correct (not a benchmark — just a sanity check)
    using namespace tml::simd;
    bool ok = true;
    ok &= is_whitespace_simd(' ') && is_whitespace_simd('\t') && is_whitespace_simd('\n');
    ok &= !is_whitespace_simd('a') && !is_whitespace_simd('0');
    ok &= is_alpha_simd('a') && is_alpha_simd('Z');
    ok &= !is_alpha_simd('0') && !is_alpha_simd(' ');
    ok &= is_digit_simd('0') && is_digit_simd('9');
    ok &= !is_digit_simd('a');
    ok &= is_hex_simd('a') && is_hex_simd('F') && is_hex_simd('0');
    ok &= !is_hex_simd('g') && !is_hex_simd('G');
    ok &= is_ident_simd('a') && is_ident_simd('_') && is_ident_simd('0');
    ok &= !is_ident_simd(' ') && !is_ident_simd('-');
    ok &= is_ident_start_simd('a') && is_ident_start_simd('_');
    ok &= !is_ident_start_simd('0');
    ok &= has_charclass('a', CHAR_LOWER | CHAR_ALPHA | CHAR_IDENT | CHAR_IDENT_START | CHAR_HEX);
    ok &= has_charclass('A', CHAR_UPPER | CHAR_ALPHA | CHAR_IDENT | CHAR_IDENT_START | CHAR_HEX);
    ok &= has_charclass(' ', CHAR_WHITESPACE);
    ok &= !has_charclass(' ', CHAR_ALPHA);

    std::printf("  Table validation: %s\n", ok ? "PASS" : "FAIL");

    // Benchmark table lookup vs naive branching
    volatile uint8_t sink = 0;
    const size_t iters = 100000;

    bench::run_bench("charclass_table_lookup_256", iters, [&]() {
        uint8_t s = 0;
        for (int c = 0; c < 256; ++c) {
            s += is_ident_simd(static_cast<uint8_t>(c));
        }
        sink = s;
    });

    bench::run_bench("charclass_bitmask_lookup_256", iters, [&]() {
        uint8_t s = 0;
        for (int c = 0; c < 256; ++c) {
            s += has_charclass(static_cast<uint8_t>(c), CHAR_IDENT);
        }
        sink = s;
    });

    (void)sink;
}

// ============================================================================
// SIMD detection benchmark (measures overhead of feature query)
// ============================================================================

static void bench_detection() {
    std::printf("=== Detection Overhead ===\n");

    volatile bool sink = false;

    bench::run_bench("has_avx2() cached query", 1000000, [&]() { sink = tml::simd::has_avx2(); });

    (void)sink;
}

// ============================================================================
// Aligned allocation benchmark
// ============================================================================

static void bench_aligned_alloc() {
    std::printf("\n=== Aligned Allocation ===\n");

    bench::run_bench("aligned_alloc_free_4KB_32align", 100000, [&]() {
        void* p = tml::simd::simd_aligned_alloc(4096, 32);
        tml::simd::simd_aligned_free(p);
    });

    bench::run_bench("aligned_alloc_free_64KB_64align", 100000, [&]() {
        void* p = tml::simd::simd_aligned_alloc(65536, 64);
        tml::simd::simd_aligned_free(p);
    });
}

// ============================================================================
// Entry point
// ============================================================================

int main() {
    std::printf("TML SIMD Benchmark Harness\n");
    std::printf("==========================\n\n");

    bench::print_features();

    bench_detection();
    bench_charclass_tables();
    bench_aligned_alloc();

    bench::print_summary();

    return 0;
}
