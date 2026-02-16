//! # Suite-Based Test Execution
//!
//! This file implements the optimized suite mode for running tests.
//!
//! ## How Suite Mode Works
//!
//! 1. Group test files by directory (compiler/tests/runtime → runtime suite)
//! 2. Compile all tests in a suite to a single DLL
//! 3. Load DLL once, run each test function in sequence
//! 4. Clean up and report results
//!
//! ## Performance Benefits
//!
//! ```text
//! Individual mode: 100 tests × (compile + load DLL) = slow
//! Suite mode:      5 suites × compile + 100 × run = fast
//! ```
//!
//! DLL loading is expensive (~20ms), so bundling tests reduces overhead.
//!
//! ## Suite Structure
//!
//! ```text
//! compiler_tests_runtime.dll
//!   ├─ tml_test_0()  // first test file
//!   ├─ tml_test_1()  // second test file
//!   └─ ...
//! ```
//!
//! ## Test Cache
//!
//! The test cache (`.test-cache.json`) tracks file hashes to skip unchanged tests.
//! When a test file hasn't changed and previously passed, it can be skipped.

#include "cli/tester/test_cache.hpp"
#include "log/log.hpp"
#include "tester_internal.hpp"
#include "types/module_binary.hpp"

#include <fstream>
#include <iomanip>
#include <regex>

namespace tml::cli::tester {

// ============================================================================
// Coverage Regression Check
// ============================================================================

/// Coverage statistics from previous report
struct PreviousCoverage {
    int covered;    // Number of functions covered
    int total;      // Total number of library functions
    double percent; // Coverage percentage
    bool valid;     // Whether the data was successfully parsed
};

/// Read the previous coverage from the existing HTML report
static PreviousCoverage get_previous_coverage(const std::string& html_path) {
    PreviousCoverage result = {0, 0, 0.0, false};

    if (!fs::exists(html_path)) {
        return result;
    }

    std::ifstream file(html_path);
    if (!file.is_open()) {
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Look for the "Functions Covered" stat card value (format: "N / M")
    // Pattern: <div class="stat-value">N / M</div>
    std::regex pattern(R"(<div class="stat-value">(\d+)\s*/\s*(\d+)</div>)");
    std::smatch match;
    if (std::regex_search(content, match, pattern)) {
        try {
            result.covered = std::stoi(match[1].str());
            result.total = std::stoi(match[2].str());
            if (result.total > 0) {
                result.percent = (100.0 * result.covered) / result.total;
            }
            result.valid = true;
        } catch (...) {
            // Keep result.valid = false
        }
    }

    return result;
}

// ============================================================================
// Suite-Based Test Execution
// ============================================================================

int run_tests_suite_mode(const std::vector<std::string>& test_files, const TestOptions& opts,
                         TestResultCollector& collector, const ColorOutput& c) {
    using Clock = std::chrono::high_resolution_clock;

    // Aggregate covered functions across all suites for library coverage analysis
    std::set<std::string> all_covered_functions;

    // Test cache for skipping unchanged tests
    // Cache file is stored in build/debug/ directory alongside .run-cache
    TestCacheManager test_cache;
    fs::path cache_file = fs::path("build") / "debug" / ".test-cache.json";
    fs::path run_cache_dir = fs::path("build") / "debug" / ".run-cache";
    bool cache_loaded = false;
    std::atomic<int> skipped_count{0};

    // Don't update cache when filter is active (partial test runs shouldn't affect cache)
    // Note: Coverage mode CAN use cache - the cache tracks coverage_enabled per entry
    // and invalidates when coverage mode changes (see can_skip_test)
    bool should_update_cache = !opts.no_cache && opts.patterns.empty();

    // Load existing cache (always load for skipping, but only update if no filter)
    // Coverage mode uses the same cache but entries are invalidated when coverage_enabled changes
    if (!opts.no_cache) {
        // Try to load cache, if it fails try to restore from temp backup
        cache_loaded = test_cache.load(cache_file.string());
        if (!cache_loaded && TestCacheManager::has_temp_backup()) {
            // Cache is missing but we have a backup - try to restore
            if (TestCacheManager::restore_from_temp(cache_file.string(), run_cache_dir.string())) {
                cache_loaded = test_cache.load(cache_file.string());
                if (cache_loaded && opts.verbose) {
                    TML_LOG_DEBUG("test", "Cache restored from backup");
                }
            }
        }
        if (opts.verbose && cache_loaded) {
            auto stats = test_cache.get_stats();
            TML_LOG_DEBUG("test", "Loaded test cache with " << stats.total_entries << " entries");
        }
    }

    try {

        // Pre-load all library modules from .tml.meta binary cache.
        // This MUST complete before any test compilation starts, otherwise
        // dynamic impl method lookups (e.g. Str::len) will fail with () return type.
        types::preload_all_meta_caches();

        // Group test files into suites
        auto phase_start = Clock::now();
        auto suites = group_tests_into_suites(test_files);
        if (opts.profile) {
            collector.profile_stats.add(
                "group_suites",
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start)
                    .count());
        }

        if (!opts.quiet) {
            TML_LOG_DEBUG("test", "Grouped into " << suites.size() << " test suite"
                                                  << (suites.size() != 1 ? "s" : ""));
        }

        // Clean up orphaned cache files periodically
        // This removes .ll, .obj, .dll files that are older than 24 hours and not from current
        // suites
        if (!opts.no_cache && fs::exists(run_cache_dir)) {
            // Only clean up once per session to avoid overhead
            static bool cleanup_done = false;
            if (!cleanup_done) {
                cleanup_done = true;

                size_t removed_count = 0;
                size_t removed_bytes = 0;
                auto now = fs::file_time_type::clock::now();
                std::vector<std::string> cleanup_extensions = {".ll", ".obj", ".pdb", ".exp",
                                                               ".lib"};

                try {
                    for (const auto& entry : fs::directory_iterator(run_cache_dir)) {
                        if (!entry.is_regular_file())
                            continue;

                        std::string ext = entry.path().extension().string();
                        bool is_cleanup_target = false;
                        for (const auto& target_ext : cleanup_extensions) {
                            if (ext == target_ext) {
                                is_cleanup_target = true;
                                break;
                            }
                        }
                        if (!is_cleanup_target)
                            continue;

                        // Check file age - remove files older than 24 hours
                        auto mod_time = fs::last_write_time(entry.path());
                        auto age = std::chrono::duration_cast<std::chrono::hours>(now - mod_time);
                        if (age.count() >= 24) {
                            try {
                                auto file_size = fs::file_size(entry.path());
                                fs::remove(entry.path());
                                ++removed_count;
                                removed_bytes += file_size;
                                if (opts.verbose) {
                                    TML_LOG_DEBUG("test", "[CLEANUP] Removed old file: "
                                                              << entry.path().filename().string());
                                }
                            } catch (...) {
                                // Skip files that can't be removed
                            }
                        }
                    }

                    if (removed_count > 0 && !opts.quiet) {
                        double mb = static_cast<double>(removed_bytes) / (1024 * 1024);
                        TML_LOG_INFO("test", "[CLEANUP] Removed "
                                                 << removed_count << " old cache files ("
                                                 << std::fixed << std::setprecision(1) << mb
                                                 << " MB)");
                    }
                } catch (const std::exception& e) {
                    if (opts.verbose) {
                        TML_LOG_WARN("test", "[CLEANUP] Error: " << e.what());
                    }
                }
            }
        }

        // Cache file hashes to avoid recomputing SHA512 multiple times
        std::map<std::string, std::string> file_hash_cache;

        // Check which suites can be entirely skipped (all tests cached)
        // This avoids compiling suites where all tests are already cached
        std::vector<TestSuite> suites_to_compile;
        std::vector<TestSuite> suites_fully_cached;

        // Skip test-result cache when using a non-default backend
        // (the cache records results from the default LLVM backend)
        bool skip_result_cache = (opts.backend != "llvm");

        if (cache_loaded && !skip_result_cache) {
            for (auto& suite : suites) {
                bool all_cached = true;
                for (const auto& test_info : suite.tests) {
                    // Check cache first before computing hash
                    if (!test_cache.can_skip(test_info.file_path)) {
                        all_cached = false;
                        break;
                    }
                    auto cached_info = test_cache.get_cached_info(test_info.file_path);
                    if (!cached_info) {
                        all_cached = false;
                        break;
                    }
                    // Only compute hash if we need to verify
                    std::string file_hash =
                        TestCacheManager::compute_file_hash(test_info.file_path);
                    file_hash_cache[test_info.file_path] = file_hash;
                    if (cached_info->sha512 != file_hash) {
                        all_cached = false;
                        break;
                    }
                }
                if (all_cached) {
                    suites_fully_cached.push_back(std::move(suite));
                } else {
                    suites_to_compile.push_back(std::move(suite));
                }
            }
        } else {
            suites_to_compile = std::move(suites);
        }

        // Process fully cached suites first (no compilation needed)
        for (auto& suite : suites_fully_cached) {
            for (const auto& test_info : suite.tests) {
                auto cached_info = test_cache.get_cached_info(test_info.file_path);
                TestResult result;
                result.file_path = test_info.file_path;
                result.test_name = test_info.test_name;
                result.group = suite.group;
                result.test_count = test_info.test_count;
                result.passed = true;
                result.duration_ms = cached_info ? cached_info->duration_ms : 0;
                result.exit_code = 0;
                if (opts.profile) {
                    collector.profile_stats.total_tests++;
                }
                collector.add(std::move(result));
                skipped_count.fetch_add(1);
            }
            if (opts.verbose) {
                TML_LOG_DEBUG("test", "Suite fully cached, skipped: " << suite.name);
            }
        }

        // Compile remaining suites IN PARALLEL
        // DLLs are loaded just-in-time during execution, not upfront, to avoid
        // holding all DLLs in memory (critical for coverage mode with large DLLs).
        struct CompiledSuite {
            TestSuite suite;
            std::string dll_path;
        };
        std::vector<CompiledSuite> compiled_suites;

        if (suites_to_compile.empty()) {
            if (!opts.quiet && !suites_fully_cached.empty()) {
                TML_LOG_DEBUG("test", "All " << suites_fully_cached.size()
                                             << " suites cached, skipping compilation");
            }
        } else {
            // Parallel suite compilation.
            // Each suite spawns internal threads for IR generation and object
            // compilation (up to hardware_concurrency() each). Running too many
            // suites in parallel causes thread explosion (N suites * M internal
            // threads) which saturates the CPU at 100%.
            // Cap at 2 concurrent suites to keep CPU usage reasonable.
            unsigned int hw = std::thread::hardware_concurrency();
            if (hw == 0)
                hw = 8;
            // Limit concurrent suite compilations: each suite uses up to hw
            // internal threads, so 2 suites already use 2*hw threads.
            unsigned int max_suite_threads = std::min(2u, hw);
            unsigned int num_compile_threads =
                std::min(max_suite_threads, static_cast<unsigned int>(suites_to_compile.size()));

            // Structure to hold compilation results
            struct CompileJob {
                size_t index;
                TestSuite suite;
                SuiteCompileResult result;
                bool compiled = false;
            };

            std::vector<CompileJob> jobs;
            jobs.reserve(suites_to_compile.size());
            for (size_t i = 0; i < suites_to_compile.size(); ++i) {
                jobs.push_back({i, std::move(suites_to_compile[i]), {}, false});
            }

            // Thread-safe job queue
            std::atomic<size_t> next_job{0};
            std::mutex output_mutex;

            // Compile suites in parallel
            auto compile_worker = [&]() {
                while (true) {
                    size_t job_idx = next_job.fetch_add(1);
                    if (job_idx >= jobs.size())
                        break;

                    auto& job = jobs[job_idx];

                    if (!opts.quiet && opts.verbose) {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        TML_LOG_DEBUG("test", "Compiling suite: " << job.suite.name << " ("
                                                                  << job.suite.tests.size()
                                                                  << " tests)");
                    }

                    // Track which suite is being compiled for crash diagnostics
                    set_crash_context("compiling", job.suite.name.c_str(), nullptr, nullptr);

                    job.result = compile_test_suite(job.suite, opts.verbose, opts.no_cache,
                                                    opts.backend, opts.features);
                    job.compiled = true;
                }
            };

            // Launch compilation threads
            if (!opts.quiet) {
                TML_LOG_DEBUG("test", "Compiling " << jobs.size() << " suites with "
                                                   << num_compile_threads << " threads...");
            }

            phase_start = Clock::now();
            std::vector<std::thread> compile_threads;
            for (unsigned int t = 0; t < std::min(num_compile_threads, (unsigned int)jobs.size());
                 ++t) {
                compile_threads.emplace_back(compile_worker);
            }
            for (auto& t : compile_threads) {
                t.join();
            }

            if (opts.profile) {
                collector.profile_stats.add("parallel_compile",
                                            std::chrono::duration_cast<std::chrono::microseconds>(
                                                Clock::now() - phase_start)
                                                .count());
            }

            // Process compilation results — collect compiled suites for deferred loading.
            for (auto& job : jobs) {
                auto& suite = job.suite;
                auto& compile_result = job.result;

                if (!compile_result.success) {
                    // Report compilation error but continue with other suites
                    TestResult error_result;
                    error_result.file_path = compile_result.failed_test;
                    error_result.test_name = fs::path(compile_result.failed_test).stem().string();
                    error_result.group = suite.group;
                    error_result.passed = false;
                    error_result.compilation_error = true;
                    error_result.exit_code = EXIT_COMPILATION_ERROR;
                    error_result.error_message =
                        "COMPILATION FAILED\n" + compile_result.error_message;

                    collector.add(std::move(error_result));

                    TML_LOG_ERROR("build", "COMPILATION FAILED suite="
                                               << suite.name
                                               << " file=" << compile_result.failed_test
                                               << " error=" << compile_result.error_message);

                    continue; // Continue with other suites instead of stopping
                }

                suite.dll_path = compile_result.dll_path;
                compiled_suites.push_back({std::move(suite), compile_result.dll_path});
            }
        } // end of suites_to_compile block

        // Run all tests from compiled suites sequentially.
        // DLLs are loaded just-in-time and unloaded after each suite to avoid
        // holding all DLLs in memory (critical for coverage mode).
        std::mutex output_mutex;
        std::mutex cache_mutex;    // For synchronized cache updates
        std::mutex coverage_mutex; // For synchronized coverage collection
        std::atomic<bool> fail_fast_triggered{false};

        // Single-threaded test execution to avoid global state conflicts
        // between test DLLs (mem_track atexit, log sinks, etc.)
        std::atomic<size_t> suite_index{0};

        // Worker function that processes suites
        auto suite_worker = [&]() {
            while (true) {
                // Check if fail-fast was triggered
                if (fail_fast_triggered.load()) {
                    return;
                }

                // Get next suite to process
                size_t idx = suite_index.fetch_add(1);
                if (idx >= compiled_suites.size()) {
                    return;
                }

                auto& compiled = compiled_suites[idx];
                auto& suite = compiled.suite;

                // Load DLL just-in-time for this suite
                set_crash_context("loading_dll", suite.name.c_str(), nullptr,
                                  compiled.dll_path.c_str());
                phase_start = Clock::now();
                DynamicLibrary lib;
                bool load_ok = lib.load(compiled.dll_path);
                if (opts.profile) {
                    collector.profile_stats.add(
                        "suite_load", std::chrono::duration_cast<std::chrono::microseconds>(
                                          Clock::now() - phase_start)
                                          .count());
                }
                if (!load_ok) {
                    TestResult error_result;
                    error_result.file_path = suite.tests.empty() ? "" : suite.tests[0].file_path;
                    error_result.test_name = suite.name;
                    error_result.group = suite.group;
                    error_result.passed = false;
                    error_result.compilation_error = true;
                    error_result.error_message = "Failed to load suite DLL: " + lib.get_error();

                    collector.add(std::move(error_result));

                    TML_LOG_ERROR("build", "DLL LOAD FAILED suite=" << suite.name << " error="
                                                                    << lib.get_error());

                    continue; // Continue with other suites
                }

                TML_LOG_INFO("test", "Running suite: " << suite.name << " (" << suite.tests.size()
                                                       << " test files, " << (idx + 1) << "/"
                                                       << compiled_suites.size() << ")");
                // Flush log to ensure crash diagnostics are captured
                tml::log::Logger::instance().flush();

                for (size_t i = 0; i < suite.tests.size(); ++i) {
                    // Check fail-fast before each test
                    if (fail_fast_triggered.load()) {
                        return;
                    }

                    const auto& test_info = suite.tests[i];

                    // Compute file hash (thread-local, no lock needed)
                    std::string file_hash =
                        TestCacheManager::compute_file_hash(test_info.file_path);

                    // Check if we can skip this test (valid cache + previously passed)
                    if (cache_loaded) {
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        if (test_cache.can_skip(test_info.file_path)) {
                            auto cached_info = test_cache.get_cached_info(test_info.file_path);
                            if (cached_info && cached_info->sha512 == file_hash) {
                                // Use cached result
                                TestResult result;
                                result.file_path = test_info.file_path;
                                result.test_name = test_info.test_name;
                                result.group = suite.group;
                                result.test_count = test_info.test_count;
                                result.passed = true;
                                result.duration_ms = cached_info->duration_ms;
                                result.exit_code = 0;

                                if (opts.profile) {
                                    collector.profile_stats.total_tests++;
                                }

                                collector.add(std::move(result));
                                skipped_count.fetch_add(1);

                                TML_LOG_DEBUG("test", "Skipped (cached): " << test_info.test_name);
                                continue;
                            }
                        }
                    }

                    // Show which test is running (INFO level for crash diagnostics)
                    TML_LOG_INFO("test", "  Test " << (i + 1) << "/" << suite.tests.size() << ": "
                                                   << test_info.test_name);
                    tml::log::Logger::instance().flush();

                    // Track which test is running for crash diagnostics (C++ side)
                    set_crash_context("running", suite.name.c_str(), test_info.test_name.c_str(),
                                      test_info.file_path.c_str());

                    // Also set crash context in the DLL's VEH handler (C side)
                    // so hardware exceptions report which test crashed
                    using TmlSetCrashCtx = void (*)(const char*, const char*, const char*);
                    auto set_dll_crash_ctx =
                        lib.get_function<TmlSetCrashCtx>("tml_set_test_crash_context");
                    if (set_dll_crash_ctx) {
                        set_dll_crash_ctx(test_info.test_name.c_str(), test_info.file_path.c_str(),
                                          suite.name.c_str());
                    }

                    // Set memory tracking context so leaked allocations
                    // report which test was active when they were created
                    using TmlMemTrackSetCtx = void (*)(const char*, const char*);
                    auto set_mem_ctx =
                        lib.get_function<TmlMemTrackSetCtx>("tml_mem_track_set_test_context");
                    if (set_mem_ctx) {
                        set_mem_ctx(test_info.test_name.c_str(), test_info.file_path.c_str());
                    }

                    // Clear crash severity from any previous test
                    using TmlClearCrashSev = void (*)();
                    auto clear_sev = lib.get_function<TmlClearCrashSev>("tml_clear_crash_severity");
                    if (clear_sev) {
                        clear_sev();
                    }

                    TestResult result;
                    result.file_path = test_info.file_path;
                    result.test_name = test_info.test_name;
                    result.group = suite.group;
                    result.test_count = test_info.test_count;

                    auto test_start = Clock::now();

                    auto run_result =
                        run_suite_test(lib, static_cast<int>(i), opts.verbose, opts.timeout_seconds,
                                       test_info.test_name, opts.backtrace);
                    int run_exit_code = run_result.exit_code;
                    bool run_success = run_result.success;

                    auto run_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                               Clock::now() - test_start)
                                               .count();

                    if (opts.profile) {
                        collector.profile_stats.add("test_run", run_duration_us);
                        collector.profile_stats.total_tests++;
                    }

                    result.duration_ms = run_duration_us / 1000;
                    result.passed = run_success;
                    result.exit_code = run_exit_code;

                    if (!result.passed) {
                        // Build clear error message with test name prominent
                        result.error_message = "\n  FAILED: " + test_info.test_name;
                        result.error_message += "\n  File:   " + test_info.file_path;
                        result.error_message += "\n  Exit:   " + std::to_string(result.exit_code);

                        if (!run_result.error.empty()) {
                            result.error_message += "\n  Error:  " + run_result.error;
                        }
                        if (!run_result.output.empty()) {
                            std::string output = run_result.output;
                            while (!output.empty() &&
                                   (output.back() == '\n' || output.back() == '\r')) {
                                output.pop_back();
                            }
                            if (!output.empty()) {
                                result.error_message += "\n  Output: " + output;
                            }
                        }
                        result.error_message += "\n";

                        // Log structured failure info for JSON log file
                        TML_LOG_ERROR("test",
                                      "FAILED test=" << test_info.test_name
                                                     << " file=" << test_info.file_path
                                                     << " exit=" << result.exit_code
                                                     << " duration_ms=" << result.duration_ms);
                        if (!run_result.error.empty()) {
                            TML_LOG_ERROR("test", "Error: " << run_result.error);
                        }
                    }

                    // Update cache with result (synchronized)
                    if (should_update_cache) {
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        std::vector<std::string> test_functions;
                        test_cache.update(
                            test_info.file_path, file_hash, suite.name, test_functions,
                            result.passed ? CachedTestStatus::Pass : CachedTestStatus::Fail,
                            result.duration_ms, {}, opts.coverage, opts.profile);
                    }

                    collector.add(std::move(result));

                    // Handle fail-fast
                    if (opts.fail_fast && !run_success) {
                        fail_fast_triggered.store(true);
                        TML_LOG_WARN("test", "Test failed, stopping due to --fail-fast");
                        return;
                    }

                    // Handle dangerous crash: abort remaining tests in this suite
                    // to avoid running on potentially corrupted state.
                    if (!run_success && run_exit_code == -2) {
                        using TmlGetAbortSuite = int32_t (*)();
                        auto get_abort =
                            lib.get_function<TmlGetAbortSuite>("tml_get_crash_abort_suite");
                        if (get_abort && get_abort()) {
                            // Get severity for logging
                            using TmlGetSeverity = int32_t (*)();
                            auto get_sev =
                                lib.get_function<TmlGetSeverity>("tml_get_crash_severity");
                            int32_t severity = get_sev ? get_sev() : 0;
                            const char* sev_names[] = {
                                "NONE",           "NULL_DEREF",      "ARITHMETIC",
                                "USE_AFTER_FREE", "WRITE_VIOLATION", "DEP_VIOLATION",
                                "STACK_OVERFLOW", "HEAP_CORRUPTION", "UNKNOWN"};
                            const char* sev_name =
                                (severity >= 0 && severity <= 8) ? sev_names[severity] : "UNKNOWN";

                            size_t remaining = suite.tests.size() - (i + 1);
                            TML_LOG_WARN("test",
                                         "Aborting suite \""
                                             << suite.name << "\" after " << sev_name
                                             << " crash — skipping " << remaining
                                             << " remaining test(s) to avoid corrupted state");

                            // Mark remaining tests as skipped
                            for (size_t skip_idx = i + 1; skip_idx < suite.tests.size();
                                 skip_idx++) {
                                auto& skip_info = suite.tests[skip_idx];
                                TestResult skip_result;
                                skip_result.file_path = skip_info.file_path;
                                skip_result.test_name = skip_info.test_name;
                                skip_result.group = suite.group;
                                skip_result.test_count = skip_info.test_count;
                                skip_result.passed = false;
                                skip_result.exit_code = -3; // special: skipped due to crash
                                skip_result.error_message =
                                    "\n  SKIPPED: " + skip_info.test_name + "\n  Reason: Prior " +
                                    sev_name + " crash in suite — state may be corrupted\n";
                                collector.add(std::move(skip_result));
                            }
                            return; // exit the suite loop
                        }
                    }
                }

                // Collect coverage data before unloading DLL (synchronized)
                if (CompilerOptions::coverage) {
                    using GetFuncCountFunc = int32_t (*)();
                    using GetFuncNameFunc = const char* (*)(int32_t);
                    using GetFuncHitsFunc = int32_t (*)(int32_t);

                    auto get_func_count = lib.get_function<GetFuncCountFunc>("tml_get_func_count");
                    auto get_func_name = lib.get_function<GetFuncNameFunc>("tml_get_func_name");
                    auto get_func_hits = lib.get_function<GetFuncHitsFunc>("tml_get_func_hits");

                    if (get_func_count && get_func_name && get_func_hits) {
                        std::lock_guard<std::mutex> lock(coverage_mutex);
                        int32_t count = get_func_count();
                        for (int32_t i = 0; i < count; i++) {
                            const char* name = get_func_name(i);
                            int32_t hits = get_func_hits(i);
                            if (name && hits > 0) {
                                all_covered_functions.insert(name);
                            }
                        }
                    }
                }

                // Clean up suite DLL
                set_crash_context("unloading_dll", suite.name.c_str(), nullptr,
                                  suite.dll_path.c_str());
                lib.unload();
                try {
                    fs::remove(suite.dll_path);
#ifdef _WIN32
                    fs::path lib_file = suite.dll_path;
                    lib_file.replace_extension(".lib");
                    if (fs::exists(lib_file)) {
                        fs::remove(lib_file);
                    }
#endif
                } catch (...) {
                    // Ignore cleanup errors
                }

                // Incremental cache save after each suite completes
                // This ensures cache is preserved even if process crashes later
                if (should_update_cache) {
                    std::lock_guard<std::mutex> lock(cache_mutex);
                    fs::create_directories(cache_file.parent_path());
                    test_cache.save(cache_file.string());
                }

                // Incremental coverage save after each suite completes
                // If the process crashes in a later suite, we still have partial data
                // NOTE: Only save covered_functions.txt here, NOT the full report.
                // The full report (HTML/JSON/history) is written once at the end.
                if (CompilerOptions::coverage && !all_covered_functions.empty()) {
                    try {
                        std::lock_guard<std::mutex> lock(coverage_mutex);
                        fs::path cov_dir = fs::path("build") / "coverage";
                        fs::create_directories(cov_dir);
                        fs::path partial_path = cov_dir / "covered_functions.txt";
                        std::ofstream cov_out(partial_path, std::ios::trunc);
                        if (cov_out) {
                            for (const auto& fn : all_covered_functions) {
                                cov_out << fn << "\n";
                            }
                        }
                    } catch (...) {
                        // Non-critical — don't let incremental save failures block tests
                    }
                }

                // Suite completed - clear crash context
                clear_crash_context();
            }
        };

        // Launch suite execution (single-threaded)
        if (!opts.quiet) {
            TML_LOG_DEBUG("test", "Running " << compiled_suites.size() << " suites...");
        }

        phase_start = Clock::now();
        suite_worker();

        if (opts.profile) {
            collector.profile_stats.add(
                "parallel_execute",
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start)
                    .count());
        }

        // Check if fail-fast was triggered
        if (fail_fast_triggered.load()) {
            // Save cache before early exit to preserve progress
            if (should_update_cache) {
                fs::create_directories(cache_file.parent_path());
                test_cache.save(cache_file.string());
            }
            return 1;
        }

        // Collect test run statistics from the results
        TestRunStats test_stats;
        test_stats.total_files = static_cast<int>(test_files.size());

        // Aggregate stats by suite
        std::map<std::string, SuiteStats> suite_map;
        for (const auto& result : collector.results) {
            auto& ss = suite_map[result.group];
            ss.name = result.group;
            ss.test_count += result.test_count;
            ss.duration_ms += result.duration_ms;
            test_stats.total_tests += result.test_count;
            test_stats.total_duration_ms += result.duration_ms;
        }

        // Convert map to vector
        for (auto& [name, ss] : suite_map) {
            test_stats.suites.push_back(std::move(ss));
        }

        // Sort suites by test count (descending)
        std::sort(
            test_stats.suites.begin(), test_stats.suites.end(),
            [](const SuiteStats& a, const SuiteStats& b) { return a.test_count > b.test_count; });

        // Check if any tests failed or crashed
        // A crash is detected by non-zero exit codes (especially negative ones on Windows)
        bool has_failures = false;
        bool has_crashes = false;
        int failure_count = 0;
        int crash_count = 0;
        int compilation_error_count = 0;
        for (const auto& result : collector.results) {
            if (!result.passed) {
                has_failures = true;
                failure_count++;
                if (result.compilation_error) {
                    compilation_error_count++;
                }
                // Detect crashes: negative exit codes (Windows STATUS codes) or
                // specific crash indicators
                if (result.exit_code < 0 || result.exit_code == -2 || result.compilation_error) {
                    has_crashes = true;
                    crash_count++;
                }
            }
        }

        // Populate failure info in test_stats for coverage logging
        test_stats.failed_count = failure_count;
        test_stats.compilation_error_count = compilation_error_count;
        test_stats.no_cache = opts.no_cache;

        // Print library coverage analysis after all suites complete
        // Note: Coverage with filters is blocked in run.cpp, so opts.patterns is always empty here
        if (CompilerOptions::coverage) {
            // Check if failures are only compilation errors (not test logic failures)
            bool has_test_failures = false;
            for (const auto& result : collector.results) {
                if (!result.passed && !result.compilation_error) {
                    has_test_failures = true;
                    break;
                }
            }

            if (has_failures) {
                // Warn about failures but still generate coverage from passing suites.
                // Partial coverage is far more useful than no coverage at all.
                std::string failure_type =
                    has_test_failures ? "test(s) failed" : "suite(s) had compilation errors";
                TML_LOG_WARN(
                    "test", c.yellow() << c.bold()
                                       << "========================================================"
                                       << c.reset());
                TML_LOG_WARN("test",
                             c.yellow()
                                 << c.bold() << "  COVERAGE WARNING: " << failure_count << " "
                                 << failure_type
                                 << (has_crashes ? " (" + std::to_string(crash_count) + " crashed)"
                                                 : "")
                                 << c.reset());
                TML_LOG_WARN("test", c.yellow()
                                         << c.bold()
                                         << "  Generating partial coverage from passing tests."
                                         << c.reset());
                TML_LOG_WARN(
                    "test", c.yellow() << c.bold()
                                       << "========================================================"
                                       << c.reset());

                // Log each failure for diagnostics
                for (const auto& result : collector.results) {
                    if (!result.passed) {
                        TML_LOG_ERROR(
                            "test",
                            "  FAILED: " << result.test_name << " (exit=" << result.exit_code << ")"
                                         << (result.compilation_error ? " [COMPILATION ERROR]" : "")
                                         << " file=" << result.file_path);
                    }
                }
            }

            {
                // Generate coverage report (even with failures — partial data is valuable)
                print_library_coverage_report(all_covered_functions, c, test_stats);

                // Write HTML report with proper library coverage data ONLY if:
                // 1. Coverage is not zero
                // 2. Coverage PERCENTAGE is not regressing from previous report
                if (!CompilerOptions::coverage_output.empty()) {
                    int current_covered = static_cast<int>(all_covered_functions.size());

                    // Never update with zero coverage - something went wrong
                    if (current_covered == 0) {
                        TML_LOG_FATAL(
                            "test",
                            c.red() << c.bold()
                                    << "========================================================"
                                    << c.reset());
                        TML_LOG_FATAL("test", c.red()
                                                  << c.bold()
                                                  << "  COVERAGE ABORTED: Zero functions tracked"
                                                  << c.reset());
                        TML_LOG_FATAL("test",
                                      c.red() << c.bold()
                                              << "  Tests ran but no coverage data was collected."
                                              << c.reset());
                        TML_LOG_FATAL(
                            "test", c.red() << c.bold()
                                            << "  This indicates a bug in coverage instrumentation."
                                            << c.reset());
                        TML_LOG_FATAL("test", c.red() << c.bold()
                                                      << "  HTML/JSON files will NOT be generated."
                                                      << c.reset());
                        TML_LOG_FATAL(
                            "test",
                            c.red() << c.bold()
                                    << "========================================================"
                                    << c.reset());
                    } else {
                        auto previous = get_previous_coverage(CompilerOptions::coverage_output);

                        // Use the previous total as reference for calculating current percentage
                        // This ensures we compare apples to apples even if library grows
                        // If no previous report, allow any coverage (as long as > 0)
                        bool should_update = true;
                        double current_percent = 0.0;

                        if (previous.valid && previous.total > 0) {
                            // Calculate current percentage using the SAME total as previous
                            // This is the fair comparison: did we cover more or fewer functions?
                            current_percent = (100.0 * current_covered) / previous.total;

                            // Regression if current percentage is less than previous
                            should_update = current_percent >= previous.percent;
                        }

                        // Always write to temp files — this ensures coverage history
                        // log is appended even when HTML/JSON won't be promoted
                        std::string tmp_output = CompilerOptions::coverage_output + ".tmp";
                        write_library_coverage_html(all_covered_functions, tmp_output, test_stats);

                        // The JSON is written alongside HTML by write_library_coverage_html
                        // with replace_extension(".json") — so for "X.html.tmp" it becomes
                        // "X.html.json". We need to find and rename that too.
                        std::string tmp_json =
                            fs::path(tmp_output).replace_extension(".json").string();
                        std::string final_json = fs::path(CompilerOptions::coverage_output)
                                                     .replace_extension(".json")
                                                     .string();

                        if (should_update) {
                            bool html_ok = fs::exists(tmp_output) && fs::file_size(tmp_output) > 0;
                            bool json_ok = fs::exists(tmp_json) && fs::file_size(tmp_json) > 0;

                            if (html_ok && json_ok) {
                                // Atomically replace final files
                                try {
                                    fs::rename(tmp_output, CompilerOptions::coverage_output);
                                    fs::rename(tmp_json, final_json);
                                    TML_LOG_INFO("test",
                                                 c.green()
                                                     << c.bold()
                                                     << "[Coverage report updated successfully]"
                                                     << c.reset());
                                } catch (const std::exception& e) {
                                    TML_LOG_FATAL(
                                        "test", "Failed to finalize coverage files: " << e.what());
                                    // Clean up temp files
                                    try {
                                        fs::remove(tmp_output);
                                    } catch (...) {}
                                    try {
                                        fs::remove(tmp_json);
                                    } catch (...) {}
                                }
                            } else {
                                TML_LOG_FATAL("test", c.red() << c.bold()
                                                              << "================================="
                                                                 "======================="
                                                              << c.reset());
                                TML_LOG_FATAL(
                                    "test",
                                    c.red()
                                        << c.bold()
                                        << "  COVERAGE ABORTED: Failed to write temp coverage files"
                                        << c.reset());
                                TML_LOG_FATAL("test",
                                              c.red() << c.bold()
                                                      << "  HTML ok: " << (html_ok ? "yes" : "NO")
                                                      << ", JSON ok: " << (json_ok ? "yes" : "NO")
                                                      << c.reset());
                                TML_LOG_FATAL("test", c.red() << c.bold()
                                                              << "================================="
                                                                 "======================="
                                                              << c.reset());
                                // Clean up temp files
                                try {
                                    fs::remove(tmp_output);
                                } catch (...) {}
                                try {
                                    fs::remove(tmp_json);
                                } catch (...) {}
                            }
                        } else {
                            // Coverage regression detected — clean up temp files
                            // (history log was already appended by write_library_coverage_html)
                            try {
                                fs::remove(tmp_output);
                            } catch (...) {}
                            try {
                                fs::remove(tmp_json);
                            } catch (...) {}

                            TML_LOG_WARN(
                                "test",
                                c.yellow()
                                    << c.bold()
                                    << "[HTML report not updated - coverage regression detected]"
                                    << c.reset());
                            TML_LOG_WARN("test", c.dim() << "   Previous: " << previous.covered
                                                         << "/" << previous.total << " functions ("
                                                         << std::fixed << std::setprecision(1)
                                                         << previous.percent << "%)" << c.reset());
                            TML_LOG_WARN("test", c.dim() << "   Current:  " << current_covered
                                                         << "/" << previous.total << " functions ("
                                                         << std::fixed << std::setprecision(1)
                                                         << current_percent << "%)" << c.reset());
                            TML_LOG_WARN("test",
                                         c.dim() << "   Run with --force-coverage to update anyway"
                                                 << c.reset());
                        }
                    }
                }
            }
        }

        // Save cache (only when not using filter to avoid partial updates)
        if (should_update_cache) {
            // Ensure build directory exists
            fs::create_directories(cache_file.parent_path());
            if (test_cache.save(cache_file.string())) {
                if (opts.verbose) {
                    auto stats = test_cache.get_stats();
                    TML_LOG_DEBUG("test",
                                  "Saved test cache with " << stats.total_entries << " entries");
                }
                // Backup cache to temp directory for recovery if cache is deleted
                TestCacheManager::backup_to_temp(cache_file.string(), run_cache_dir.string());
            }
        } else if (opts.verbose && !opts.patterns.empty()) {
            TML_LOG_DEBUG("test", "Cache not updated (filter active)");
        }

        // Report skipped tests
        int skipped = skipped_count.load();
        if (skipped > 0 && !opts.quiet) {
            TML_LOG_DEBUG("test", "Skipped " << skipped << " cached test"
                                             << (skipped != 1 ? "s" : "") << " (unchanged)");
        }

        return 0;

    } catch (const std::exception& e) {
        // Save cache before exit to preserve progress
        if (should_update_cache) {
            try {
                fs::create_directories(cache_file.parent_path());
                test_cache.save(cache_file.string());
            } catch (...) {}
        }
        TML_LOG_FATAL("test", "Exception in run_tests_suite_mode: " << e.what());
        return 1;
    } catch (...) {
        // Save cache before exit to preserve progress
        if (should_update_cache) {
            try {
                fs::create_directories(cache_file.parent_path());
                test_cache.save(cache_file.string());
            } catch (...) {}
        }
        TML_LOG_FATAL("test", "Unknown exception in run_tests_suite_mode");
        return 1;
    }
}

} // namespace tml::cli::tester
