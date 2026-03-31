//! # TML SIMD Comprehensive Benchmark Suite
//!
//! Runs benchmarks for ALL SIMD-optimized subsystems:
//!   - Distance functions (f32, f64) — Phase 2
//!   - String operations (find, case, trim, hash) — Phase 3
//!   - Math (sum_i32, sum_f64) — Phase 6
//!   - Infrastructure (charclass, detection, allocation)
//!
//! Outputs:
//!   - Human-readable table to stdout
//!   - JSON results to build/bench/results.json
//!
//! Build:
//!   scripts\build.bat --bench
//!
//! Run:
//!   build/debug/bin/tml_bench.exe [--json path/to/output.json]

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "search/hnsw_index.hpp"
#include "search/simd_distance.hpp"
#include "simd/simd_charclass.h"
#include "simd/simd_detect.hpp"
#include "simd/simd_math.h"
#include "simd/simd_string.h"
#include "simd/simd_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
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

    size_t n = samples.size();
    if (n % 2 == 0) {
        s.median_ns = static_cast<double>(samples[n / 2 - 1] + samples[n / 2]) / 2.0;
    } else {
        s.median_ns = static_cast<double>(samples[n / 2]);
    }

    double sum = 0;
    for (auto v : samples)
        sum += static_cast<double>(v);
    s.mean_ns = sum / static_cast<double>(n);

    double var_sum = 0;
    for (auto v : samples) {
        double d = static_cast<double>(v) - s.mean_ns;
        var_sum += d * d;
    }
    s.stddev_ns = std::sqrt(var_sum / static_cast<double>(n));

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
    std::string category;
    int input_size;
    Stats stats;
};

static std::vector<BenchResult> g_results;
static std::string g_current_category;

static void set_category(const char* cat) {
    g_current_category = cat;
}

template <typename Func>
void run_bench(const char* name, size_t iterations, Func func, int input_size = 0) {
    // Warm-up: 10% of iterations (minimum 10)
    size_t warmup = std::max<size_t>(iterations / 10, 10);
    for (size_t i = 0; i < warmup; ++i) {
        func();
    }

    std::vector<int64_t> samples;
    samples.reserve(iterations);

    for (size_t i = 0; i < iterations; ++i) {
        auto t0 = now_ns();
        func();
        auto t1 = now_ns();
        samples.push_back(t1 - t0);
    }

    auto stats = compute_stats(samples);

    std::printf("  %-40s  min=%8.1f  median=%8.1f  p95=%8.1f  stddev=%7.1f ns  (%zu iters)\n", name,
                stats.min_ns, stats.median_ns, stats.p95_ns, stats.stddev_ns, iterations);

    g_results.push_back({name, g_current_category, input_size, stats});
}

// Print summary table
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

// Write JSON results to file
static void write_json(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) {
        std::fprintf(stderr, "ERROR: cannot open %s for writing\n", path);
        return;
    }

    std::fprintf(f, "[\n");
    for (size_t i = 0; i < g_results.size(); ++i) {
        auto& r = g_results[i];
        std::fprintf(f,
                     "  {\"function\": \"%s\", \"category\": \"%s\", \"input_size\": %d, "
                     "\"median_ns\": %.1f, \"p95_ns\": %.1f, \"min_ns\": %.1f, "
                     "\"max_ns\": %.1f, \"stddev_ns\": %.1f, \"iterations\": %zu}%s\n",
                     r.name.c_str(), r.category.c_str(), r.input_size, r.stats.median_ns,
                     r.stats.p95_ns, r.stats.min_ns, r.stats.max_ns, r.stats.stddev_ns,
                     r.stats.iterations, (i + 1 < g_results.size()) ? "," : "");
    }
    std::fprintf(f, "]\n");
    fclose(f);
    std::printf("\nJSON results written to: %s\n", path);
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
// Helper: fill arrays with deterministic data
// ============================================================================

static void fill_float(float* arr, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        arr[i] = static_cast<float>(i % 1000) * 0.001f + 0.1f;
    }
}

static void fill_double(double* arr, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        arr[i] = static_cast<double>(i % 1000) * 0.001 + 0.1;
    }
}

static void fill_i32(int32_t* arr, int n) {
    for (int i = 0; i < n; ++i) {
        arr[i] = static_cast<int32_t>((i * 7 + 13) % 1000 - 500);
    }
}

// ============================================================================
// Distance Benchmarks (Phase 2)
// ============================================================================

static void bench_distance() {
    bench::set_category("distance");
    std::printf("=== Distance Functions (f32) ===\n");

    const size_t dims[] = {64, 128, 256, 512, 1024};
    const size_t iters = 50000;

    for (auto dim : dims) {
        auto* a = static_cast<float*>(tml::simd::simd_aligned_alloc(dim * sizeof(float), 32));
        auto* b = static_cast<float*>(tml::simd::simd_aligned_alloc(dim * sizeof(float), 32));
        fill_float(a, dim);
        fill_float(b, dim);

        volatile float sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "dot_product_f32_%zu", dim);
        bench::run_bench(
            name, iters, [&]() { sink = tml::search::dot_product_f32(a, b, dim); },
            static_cast<int>(dim));

        std::snprintf(name, sizeof(name), "cosine_sim_f32_%zu", dim);
        bench::run_bench(
            name, iters, [&]() { sink = tml::search::cosine_similarity_f32(a, b, dim); },
            static_cast<int>(dim));

        std::snprintf(name, sizeof(name), "l2_squared_f32_%zu", dim);
        bench::run_bench(
            name, iters, [&]() { sink = tml::search::l2_distance_squared_f32(a, b, dim); },
            static_cast<int>(dim));

        (void)sink;
        tml::simd::simd_aligned_free(a);
        tml::simd::simd_aligned_free(b);
    }

    std::printf("\n=== Distance Functions (f64) ===\n");
    for (auto dim : dims) {
        auto* a = static_cast<double*>(tml::simd::simd_aligned_alloc(dim * sizeof(double), 32));
        auto* b = static_cast<double*>(tml::simd::simd_aligned_alloc(dim * sizeof(double), 32));
        fill_double(a, dim);
        fill_double(b, dim);

        volatile double sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "dot_product_f64_%zu", dim);
        bench::run_bench(
            name, iters, [&]() { sink = tml::search::dot_product_f64(a, b, dim); },
            static_cast<int>(dim));

        std::snprintf(name, sizeof(name), "cosine_sim_f64_%zu", dim);
        bench::run_bench(
            name, iters, [&]() { sink = tml::search::cosine_similarity_f64(a, b, dim); },
            static_cast<int>(dim));

        (void)sink;
        tml::simd::simd_aligned_free(a);
        tml::simd::simd_aligned_free(b);
    }
}

// ============================================================================
// String Benchmarks (Phase 3)
// ============================================================================

static void bench_string() {
    bench::set_category("string");
    std::printf("\n=== String Search ===\n");

    const size_t iters = 50000;

    // Create haystack of various sizes
    const int hay_sizes[] = {64, 1024, 65536};
    for (auto hay_size : hay_sizes) {
        std::string haystack(static_cast<size_t>(hay_size), 'x');
        // Place needle near the end so we exercise the full scan
        const char* needle = "findme";
        int needle_len = 6;
        if (hay_size >= needle_len + 10) {
            std::memcpy(&haystack[hay_size - needle_len - 5], needle,
                        static_cast<size_t>(needle_len));
        }

        volatile int sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "str_find_%dB_needle6", hay_size);
        bench::run_bench(
            name, iters,
            [&]() { sink = simd_str_find(haystack.c_str(), hay_size, needle, needle_len); },
            hay_size);

        std::snprintf(name, sizeof(name), "str_rfind_%dB_needle6", hay_size);
        bench::run_bench(
            name, iters,
            [&]() { sink = simd_str_rfind(haystack.c_str(), hay_size, needle, needle_len); },
            hay_size);

        std::snprintf(name, sizeof(name), "str_contains_%dB_needle6", hay_size);
        bench::run_bench(
            name, iters,
            [&]() { sink = simd_str_contains(haystack.c_str(), hay_size, needle, needle_len); },
            hay_size);

        (void)sink;
    }

    std::printf("\n=== Case Conversion ===\n");
    const int case_sizes[] = {16, 256, 4096, 65536};
    for (auto sz : case_sizes) {
        std::string src(static_cast<size_t>(sz), 'a');
        std::string dst(static_cast<size_t>(sz), '\0');
        char name[80];

        std::snprintf(name, sizeof(name), "to_upper_%dB", sz);
        bench::run_bench(name, iters, [&]() { simd_to_upper(src.c_str(), &dst[0], sz); }, sz);

        std::snprintf(name, sizeof(name), "to_lower_%dB", sz);
        bench::run_bench(name, iters, [&]() { simd_to_lower(src.c_str(), &dst[0], sz); }, sz);
    }

    std::printf("\n=== Trim ===\n");
    // String with 50% leading/trailing whitespace
    for (auto sz : case_sizes) {
        std::string s(static_cast<size_t>(sz), ' ');
        // Fill middle with non-ws
        size_t start = static_cast<size_t>(sz) / 4;
        size_t end = static_cast<size_t>(sz) * 3 / 4;
        for (size_t i = start; i < end; ++i)
            s[i] = 'a';

        volatile int sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "trim_start_%dB_50ws", sz);
        bench::run_bench(name, iters, [&]() { sink = simd_trim_start(s.c_str(), sz); }, sz);

        std::snprintf(name, sizeof(name), "trim_end_%dB_50ws", sz);
        bench::run_bench(name, iters, [&]() { sink = simd_trim_end(s.c_str(), sz); }, sz);

        (void)sink;
    }

    std::printf("\n=== String Hashing ===\n");
    const int hash_sizes[] = {4, 16, 64, 256, 1024};
    for (auto sz : hash_sizes) {
        std::string s(static_cast<size_t>(sz), 'k');
        volatile uint64_t sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "str_hash_%dB", sz);
        bench::run_bench(name, iters, [&]() { sink = simd_str_hash(s.c_str(), sz); }, sz);

        (void)sink;
    }

    std::printf("\n=== String Compare ===\n");
    for (auto sz : case_sizes) {
        std::string a(static_cast<size_t>(sz), 'a');
        std::string b(static_cast<size_t>(sz), 'a');
        volatile int sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "str_eq_%dB_match", sz);
        bench::run_bench(
            name, iters, [&]() { sink = simd_str_eq(a.c_str(), sz, b.c_str(), sz); }, sz);

        (void)sink;
    }
}

// ============================================================================
// Math Benchmarks (Phase 6)
// ============================================================================

static void bench_math() {
    bench::set_category("math");
    std::printf("\n=== Math Sum (i32) ===\n");

    const int sizes[] = {16, 256, 4096, 65536, 1048576};
    const size_t iters = 10000;

    for (auto sz : sizes) {
        auto* data = new int32_t[sz];
        fill_i32(data, sz);
        volatile int32_t sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "sum_i32_%d", sz);
        bench::run_bench(name, iters, [&]() { sink = simd_sum_i32(data, sz); }, sz);

        (void)sink;
        delete[] data;
    }

    std::printf("\n=== Math Sum (f64) ===\n");
    for (auto sz : sizes) {
        auto* data = new double[sz];
        fill_double(data, static_cast<size_t>(sz));
        volatile double sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "sum_f64_%d", sz);
        bench::run_bench(name, iters, [&]() { sink = simd_sum_f64(data, sz); }, sz);

        (void)sink;
        delete[] data;
    }

    std::printf("\n=== Math Dot Product (f64, via search) ===\n");
    const int dot_dims[] = {64, 256, 1024};
    for (auto dim : dot_dims) {
        auto* a = new double[dim];
        auto* b = new double[dim];
        fill_double(a, static_cast<size_t>(dim));
        fill_double(b, static_cast<size_t>(dim));
        volatile double sink = 0;
        char name[80];

        std::snprintf(name, sizeof(name), "dot_f64_math_%d", dim);
        bench::run_bench(name, iters, [&]() { sink = simd_dot_f64(a, b, dim); }, dim);

        (void)sink;
        delete[] a;
        delete[] b;
    }
}

// ============================================================================
// Infrastructure Benchmarks (Phase 1)
// ============================================================================

static void bench_infrastructure() {
    bench::set_category("infrastructure");
    std::printf("\n=== Detection Overhead ===\n");

    volatile bool sink = false;
    bench::run_bench("has_avx2() cached query", 1000000, [&]() { sink = tml::simd::has_avx2(); });
    (void)sink;

    std::printf("\n=== Character Classification ===\n");

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

    volatile uint8_t csink = 0;
    const size_t iters = 100000;

    bench::run_bench("charclass_table_lookup_256", iters, [&]() {
        uint8_t s = 0;
        for (int c = 0; c < 256; ++c) {
            s += is_ident_simd(static_cast<uint8_t>(c));
        }
        csink = s;
    });

    bench::run_bench("charclass_bitmask_lookup_256", iters, [&]() {
        uint8_t s = 0;
        for (int c = 0; c < 256; ++c) {
            s += has_charclass(static_cast<uint8_t>(c), CHAR_IDENT);
        }
        csink = s;
    });
    (void)csink;

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
// Lexer Benchmarks (Phase 5.3)
// ============================================================================

static void bench_lexer() {
    bench::set_category("lexer");
    std::printf("\n=== Lexer SIMD Throughput ===\n");

    // --- skip_whitespace: heavily indented source ---
    // 80 lines each indented with 16 spaces followed by a simple statement.
    {
        std::string src;
        src.reserve(4096);
        for (int i = 0; i < 80; ++i) {
            src += "                let x = 42\n"; // 16 spaces + statement
        }
        const int byte_len = static_cast<int>(src.size());

        bench::run_bench(
            "skip_whitespace (indented 80 lines)", 500,
            [&]() {
                tml::lexer::Source source = tml::lexer::Source("<bench_ws>", src);
                tml::lexer::Lexer lexer(source);
                volatile size_t n = lexer.tokenize().size();
                (void)n;
            },
            byte_len);
    }

    // --- lex_identifier: source full of long identifiers ---
    // 200 declarations with 32-character identifiers.
    {
        std::string src;
        src.reserve(8192);
        for (int i = 0; i < 200; ++i) {
            // 32-char identifier (ident_start + 31 ident_continue chars)
            src += "let abcdefghijklmnopqrstuvwxyzAB = 0\n";
        }
        const int byte_len = static_cast<int>(src.size());

        bench::run_bench(
            "lex_identifier (200 x 32-char idents)", 500,
            [&]() {
                tml::lexer::Source source = tml::lexer::Source("<bench_ident>", src);
                tml::lexer::Lexer lexer(source);
                volatile size_t n = lexer.tokenize().size();
                (void)n;
            },
            byte_len);
    }

    // --- lex_string: source full of long string literals ---
    // 100 lines each with a ~60-char string literal.
    {
        std::string src;
        src.reserve(8192);
        for (int i = 0; i < 100; ++i) {
            src += "let s = \"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01\"\n";
        }
        const int byte_len = static_cast<int>(src.size());

        bench::run_bench(
            "lex_string (100 x 60-char string literals)", 500,
            [&]() {
                tml::lexer::Source source = tml::lexer::Source("<bench_str>", src);
                tml::lexer::Lexer lexer(source);
                volatile size_t n = lexer.tokenize().size();
                (void)n;
            },
            byte_len);
    }

    // --- Full lexer throughput: real TML file ---
    // Try to read lib/core/src/str.tml; fall back to a synthetic 4 KB block.
    {
        std::string file_src;
        bool loaded = false;
        {
            std::ifstream f("lib/core/src/str.tml", std::ios::binary);
            if (f) {
                std::ostringstream ss;
                ss << f.rdbuf();
                file_src = ss.str();
                loaded = true;
            }
        }
        if (!loaded) {
            // Synthetic fallback: mix of identifiers, numbers, strings, ops
            file_src.reserve(4096);
            for (int i = 0; i < 60; ++i) {
                file_src += "func foo(x: I64, y: I64) -> I64 { let z = x + y\n"
                            "    let s = \"hello world\"\n"
                            "    z\n"
                            "}\n";
            }
        }
        const int byte_len = static_cast<int>(file_src.size());
        const char* label = loaded ? "full_lex str.tml (real file)" : "full_lex synthetic 4KB";

        bench::run_bench(
            label, 200,
            [&]() {
                tml::lexer::Source source = tml::lexer::Source("<bench_file>", file_src);
                tml::lexer::Lexer lexer(source);
                volatile size_t n = lexer.tokenize().size();
                (void)n;
            },
            byte_len);
    }

    // --- End-to-end tokenize() wall time ---
    // Same file, measuring only the tokenize() call (Source construction outside loop).
    {
        std::string file_src;
        {
            std::ifstream f("lib/core/src/str.tml", std::ios::binary);
            if (f) {
                std::ostringstream ss;
                ss << f.rdbuf();
                file_src = ss.str();
            } else {
                for (int i = 0; i < 60; ++i) {
                    file_src += "func foo(x: I64, y: I64) -> I64 { let z = x + y\n"
                                "    z\n}\n";
                }
            }
        }
        const int byte_len = static_cast<int>(file_src.size());
        tml::lexer::Source source = tml::lexer::Source("<bench_e2e>", file_src);

        bench::run_bench(
            "tokenize() call only (Source pre-built)", 500,
            [&]() {
                tml::lexer::Lexer lexer(source);
                volatile size_t n = lexer.tokenize().size();
                (void)n;
            },
            byte_len);
    }
}

// ============================================================================
// HNSW End-to-End Benchmark (Phase 2.4.5)
// ============================================================================

static void bench_hnsw() {
    bench::set_category("hnsw");
    std::printf("\n=== HNSW End-to-End ===\n");

    constexpr size_t DIMS = 128;
    constexpr size_t N_DOCS = 1000;
    constexpr size_t K = 10;

    // Pre-generate random vectors outside the timed region.
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<std::vector<float>> corpus(N_DOCS, std::vector<float>(DIMS));
    for (auto& vec : corpus) {
        for (auto& v : vec)
            v = dist(rng);
    }

    std::vector<float> query(DIMS);
    for (auto& v : query)
        v = dist(rng);

    // --- Build benchmark: insert N_DOCS vectors ---
    bench::run_bench(
        "hnsw_build_1K_128d (full insert)", 10,
        [&]() {
            tml::search::HnswIndex idx(DIMS);
            idx.set_params(16, 200, 50);
            for (size_t i = 0; i < N_DOCS; ++i) {
                idx.insert(static_cast<uint32_t>(i), corpus[i]);
            }
            volatile size_t s = idx.size();
            (void)s;
        },
        static_cast<int>(N_DOCS * DIMS * sizeof(float)));

    // Build a persistent index for query benchmarks.
    tml::search::HnswIndex idx(DIMS);
    idx.set_params(16, 200, 50);
    for (size_t i = 0; i < N_DOCS; ++i) {
        idx.insert(static_cast<uint32_t>(i), corpus[i]);
    }

    // --- Query benchmark: top-K search ---
    bench::run_bench(
        "hnsw_search_top10 (1K indexed, 128d)", 1000,
        [&]() {
            auto results = idx.search(query, K);
            volatile size_t n = results.size();
            (void)n;
        },
        static_cast<int>(DIMS * sizeof(float)));

    // --- Varied query benchmark: different query vectors ---
    bench::run_bench(
        "hnsw_search_top10 (varied queries)", 1000,
        [&]() {
            // Rotate query slightly to avoid branch predictor lock-in.
            static size_t qidx = 0;
            auto results = idx.search(corpus[qidx % N_DOCS], K);
            volatile size_t n = results.size();
            (void)n;
            ++qidx;
        },
        static_cast<int>(DIMS * sizeof(float)));
}

// ============================================================================
// Entry point
// ============================================================================

int main(int argc, char** argv) {
    std::printf("TML SIMD Comprehensive Benchmark Suite\n");
    std::printf("=======================================\n\n");

    // Parse --json flag
    const char* json_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--json") == 0 && i + 1 < argc) {
            json_path = argv[i + 1];
            ++i;
        }
    }

    bench::print_features();

    // Run all benchmark suites
    bench_distance();
    bench_string();
    bench_math();
    bench_infrastructure();
    bench_lexer();
    bench_hnsw();

    // Print summary
    bench::print_summary();

    // Write JSON if requested (or default path)
    if (json_path) {
        bench::write_json(json_path);
    } else {
        // Default output path
        bench::write_json("build/bench/results.json");
    }

    return 0;
}
