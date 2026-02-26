TML_MODULE("test")

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

#include <condition_variable>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <regex>
#include <thread>

#ifdef _WIN32
#include <process.h> // _beginthreadex
#include <windows.h>
#endif

namespace tml::cli::tester {

// ============================================================================
// Thread with custom stack size (Windows)
// ============================================================================
// std::thread uses the default 1 MB stack on Windows. Test execution needs a
// larger stack because OpenSSL DH/RSA operations use deep call chains with
// heavy BIGNUM stack usage (prime generation, modular exponentiation).
// Without this, DH tests intermittently hit EXCEPTION_STACK_OVERFLOW.

#ifdef _WIN32

/// RAII wrapper for a native Windows thread with a custom stack size.
/// Provides a join() interface compatible with std::thread usage patterns.
class NativeThread {
public:
    NativeThread() = default;

    template <typename Fn> NativeThread(Fn&& fn, size_t stack_size) {
        // Wrap the callable in a type-erased invocation
        auto* ctx = new std::function<void()>(std::forward<Fn>(fn));
        handle_ =
            reinterpret_cast<HANDLE>(_beginthreadex(nullptr,                           // security
                                                    static_cast<unsigned>(stack_size), // stack size
                                                    &NativeThread::thread_proc, // start routine
                                                    ctx,                        // argument
                                                    STACK_SIZE_PARAM_IS_A_RESERVATION, // flags
                                                    nullptr                            // thread id
                                                    ));
        if (!handle_) {
            delete ctx;
            throw std::runtime_error("Failed to create thread with custom stack size");
        }
    }

    NativeThread(NativeThread&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    NativeThread& operator=(NativeThread&& other) noexcept {
        if (this != &other) {
            if (handle_)
                join();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~NativeThread() {
        if (handle_)
            join();
    }

    bool joinable() const {
        return handle_ != nullptr;
    }

    void join() {
        if (handle_) {
            WaitForSingleObject(handle_, INFINITE);
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }

private:
    HANDLE handle_ = nullptr;

    static unsigned __stdcall thread_proc(void* arg) {
        auto* fn = static_cast<std::function<void()>*>(arg);
        (*fn)();
        delete fn;
        return 0;
    }
};

#endif // _WIN32

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
        // Individual mode: 1 test per DLL for easier debugging
        // Suite mode: up to 8 tests per DLL for parallelization
        // WORKAROUND: Force individual mode for compiler tests to avoid suite merging codegen bug
        // (See: rulebook/tasks/fix-suite-codegen-bug/)
        auto phase_start = Clock::now();
        bool has_compiler_tests =
            std::any_of(test_files.begin(), test_files.end(), [](const auto& file) {
                return file.find("compiler") != std::string::npos &&
                       file.find("test") != std::string::npos;
            });
        size_t max_per_suite = (opts.suite_mode && !has_compiler_tests) ? 8 : 1;
        auto suites = group_tests_into_suites(test_files, max_per_suite);
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
            static std::once_flag cleanup_flag;
            std::call_once(cleanup_flag, [&]() {
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
            });
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

        // ======================================================================
        // Pipeline: compile suites (3 threads) + execute (1 thread) concurrently
        // ======================================================================
        // Suites are pushed to a ready queue as they finish compiling.
        // A single execution thread consumes from the queue, loading DLLs
        // and running tests while other suites are still being compiled.
        // Execution must be single-threaded (mem_track, atexit, log sinks).

        struct CompiledSuite {
            TestSuite suite;
            std::string dll_path;
        };

        // Ready queue: compiled suites waiting to be executed
        std::vector<CompiledSuite> ready_queue;
        std::mutex ready_mutex;
        std::condition_variable ready_cv;
        std::atomic<bool> all_compiled{false};

        // Shared state for execution
        std::mutex cache_mutex;
        std::mutex coverage_mutex;
        std::atomic<bool> fail_fast_triggered{false};
        std::atomic<size_t> suites_executed{0};
        size_t total_suites_to_run = 0; // set after we know how many will compile

        // Execution thread pool synchronization
        // The bridge thread transfers compiled suites from ready_queue to exec_queue,
        // and N exec worker threads process suites in parallel.
        std::mutex exec_queue_mutex;
        std::condition_variable exec_queue_cv;
        std::atomic<bool> exec_enqueue_done{false};
        std::atomic<bool> exec_worker_crashed{false};
        std::string crashed_test_file;        // written under exec_queue_mutex
        std::string crashed_test_output;      // captured output from crashed test
        std::deque<CompiledSuite> exec_queue; // queue for execution workers

        auto pipeline_start = Clock::now();

        // --- Bridge worker: transfer compiled suites from ready_queue to exec_queue ---
        auto bridge_worker = [&]() {
            while (true) {
                CompiledSuite compiled;
                {
                    std::unique_lock<std::mutex> lock(ready_mutex);
                    ready_cv.wait(lock, [&]() {
                        return !ready_queue.empty() || all_compiled.load() ||
                               fail_fast_triggered.load();
                    });
                    if (fail_fast_triggered.load())
                        break;
                    if (ready_queue.empty()) {
                        if (all_compiled.load())
                            break;
                        continue;
                    }
                    compiled = std::move(ready_queue.front());
                    ready_queue.erase(ready_queue.begin());
                }
                // Enqueue to execution pool
                {
                    std::lock_guard<std::mutex> lock(exec_queue_mutex);
                    exec_queue.push_back(std::move(compiled));
                }
                exec_queue_cv.notify_one();
            }
            // Signal exec workers that enqueueing is done
            exec_enqueue_done.store(true);
            exec_queue_cv.notify_all();
        };

        // --- Execution worker pool: process suites from exec_queue in parallel ---
        auto exec_worker = [&]() {
            while (true) {
                CompiledSuite compiled;

                // Wait for a suite to be ready or done signal
                {
                    std::unique_lock<std::mutex> lock(exec_queue_mutex);
                    exec_queue_cv.wait(lock, [&]() {
                        return !exec_queue.empty() || exec_enqueue_done.load() ||
                               fail_fast_triggered.load() || exec_worker_crashed.load();
                    });

                    if (fail_fast_triggered.load() || exec_worker_crashed.load())
                        return;

                    if (exec_queue.empty()) {
                        if (exec_enqueue_done.load())
                            return; // No more suites coming
                        continue;   // Spurious wakeup
                    }

                    compiled = std::move(exec_queue.front());
                    exec_queue.pop_front();
                }

                auto& suite = compiled.suite;
                suites_executed.fetch_add(1);

                // Load DLL just-in-time for this suite
                set_crash_context("loading_dll", suite.name.c_str(), nullptr,
                                  compiled.dll_path.c_str());
                auto load_start = Clock::now();
                DynamicLibrary lib;
                bool load_ok = lib.load(compiled.dll_path);
                if (opts.profile) {
                    collector.profile_stats.add(
                        "suite_load", std::chrono::duration_cast<std::chrono::microseconds>(
                                          Clock::now() - load_start)
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
                    continue;
                }

                // Suite info suppressed - only show per-test results

                for (size_t i = 0; i < suite.tests.size(); ++i) {
                    if (fail_fast_triggered.load())
                        return;

                    const auto& test_info = suite.tests[i];

                    std::string file_hash =
                        TestCacheManager::compute_file_hash(test_info.file_path);

                    if (cache_loaded) {
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        if (test_cache.can_skip(test_info.file_path)) {
                            auto cached_info = test_cache.get_cached_info(test_info.file_path);
                            if (cached_info && cached_info->sha512 == file_hash) {
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

                    TML_LOG_DEBUG("test", "  Test " << (i + 1) << "/" << suite.tests.size() << ": "
                                                    << test_info.test_name);

                    // Write crash marker to disk — survives process death from fatal
                    // crashes (HEAP_CORRUPTION, etc.) that kill before SEH can run.
                    // Use thread ID in filename for multi-threaded execution pool.
                    {
                        auto tid_str = std::to_string(
                            std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000);
                        auto marker_path =
                            fs::path("build") / "debug" / (".current_test_" + tid_str);
                        std::ofstream marker(marker_path, std::ios::trunc);
                        if (marker) {
                            marker << test_info.test_name << "\n"
                                   << test_info.file_path << "\n"
                                   << suite.name << "\n";
                            marker.flush();
                        }
                    }

                    set_crash_context("running", suite.name.c_str(), test_info.test_name.c_str(),
                                      test_info.file_path.c_str());

                    using TmlSetCrashCtx = void (*)(const char*, const char*, const char*);
                    auto set_dll_crash_ctx =
                        lib.get_function<TmlSetCrashCtx>("tml_set_test_crash_context");
                    if (set_dll_crash_ctx) {
                        set_dll_crash_ctx(test_info.test_name.c_str(), test_info.file_path.c_str(),
                                          suite.name.c_str());
                    }

                    using TmlMemTrackSetCtx = void (*)(const char*, const char*);
                    auto set_mem_ctx =
                        lib.get_function<TmlMemTrackSetCtx>("tml_mem_track_set_test_context");
                    if (set_mem_ctx) {
                        set_mem_ctx(test_info.test_name.c_str(), test_info.file_path.c_str());
                    }

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

                    // Log which test is running
                    TML_LOG_INFO("test", "[RUN] "
                                             << test_info.test_name << " from "
                                             << fs::path(test_info.file_path).filename().string());

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

                        TML_LOG_ERROR("test",
                                      "FAILED test=" << test_info.test_name
                                                     << " file=" << test_info.file_path
                                                     << " exit=" << result.exit_code
                                                     << " duration_ms=" << result.duration_ms);
                        if (!run_result.error.empty()) {
                            TML_LOG_ERROR("test", "Error: " << run_result.error);
                        }
                    }

                    if (should_update_cache) {
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        std::vector<std::string> test_functions;
                        test_cache.update(
                            test_info.file_path, file_hash, suite.name, test_functions,
                            result.passed ? CachedTestStatus::Pass : CachedTestStatus::Fail,
                            result.duration_ms, {}, opts.coverage, opts.profile);
                    }

                    collector.add(std::move(result));

                    // Clear crash marker — test completed (passed or failed gracefully)
                    {
                        auto tid_str = std::to_string(
                            std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000);
                        auto marker_path =
                            fs::path("build") / "debug" / (".current_test_" + tid_str);
                        std::error_code ec;
                        fs::remove(marker_path, ec);
                    }

                    if (opts.fail_fast && !run_success) {
                        fail_fast_triggered.store(true);
                        TML_LOG_WARN("test", "Test failed, stopping due to --fail-fast");
                        return;
                    }

                    if (!run_success && run_exit_code == -2) {
                        using TmlGetAbortSuite = int32_t (*)();
                        auto get_abort =
                            lib.get_function<TmlGetAbortSuite>("tml_get_crash_abort_suite");
                        if (get_abort && get_abort()) {
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

                            for (size_t skip_idx = i + 1; skip_idx < suite.tests.size();
                                 skip_idx++) {
                                auto& skip_info = suite.tests[skip_idx];
                                TestResult skip_result;
                                skip_result.file_path = skip_info.file_path;
                                skip_result.test_name = skip_info.test_name;
                                skip_result.group = suite.group;
                                skip_result.test_count = skip_info.test_count;
                                skip_result.passed = false;
                                skip_result.exit_code = -3;
                                skip_result.error_message =
                                    "\n  SKIPPED: " + skip_info.test_name + "\n  Reason: Prior " +
                                    sev_name + " crash in suite — state may be corrupted\n";
                                collector.add(std::move(skip_result));
                            }
                            break; // exit the test loop for this suite
                        }
                    }
                }

                // Collect coverage data before unloading DLL
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
                        for (int32_t fi = 0; fi < count; fi++) {
                            const char* name = get_func_name(fi);
                            int32_t hits = get_func_hits(fi);
                            if (name && hits > 0) {
                                all_covered_functions.insert(name);
                            }
                        }
                    }
                }

                // Collect per-file leak data before unloading DLL
                if (opts.check_leaks) {
                    using TmlMemGetLeakFiles =
                        int32_t (*)(const char**, int32_t*, int64_t*, int32_t);
                    auto get_leak_files =
                        lib.get_function<TmlMemGetLeakFiles>("tml_mem_get_leak_files");

                    if (get_leak_files) {
                        constexpr int32_t MAX_LEAK_FILES = 128;
                        const char* files[MAX_LEAK_FILES];
                        int32_t counts[MAX_LEAK_FILES];
                        int64_t bytes[MAX_LEAK_FILES];

                        int32_t num_files = get_leak_files(files, counts, bytes, MAX_LEAK_FILES);
                        int32_t entries = num_files < MAX_LEAK_FILES ? num_files : MAX_LEAK_FILES;

                        if (entries > 0) {
                            std::lock_guard<std::mutex> lock(collector.mutex);
                            for (int32_t fi = 0; fi < entries; fi++) {
                                collector.leak_stats.add(files[fi] ? files[fi] : "(unknown)",
                                                         counts[fi], bytes[fi]);
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
                if (should_update_cache) {
                    std::lock_guard<std::mutex> lock(cache_mutex);
                    fs::create_directories(cache_file.parent_path());
                    test_cache.save(cache_file.string());
                }

                // Incremental coverage save after each suite completes
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
                        // Non-critical
                    }
                }

                clear_crash_context();

                // Final cleanup of crash marker after suite completes
                {
                    auto tid_str = std::to_string(
                        std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000);
                    auto marker_path = fs::path("build") / "debug" / (".current_test_" + tid_str);
                    std::error_code ec;
                    fs::remove(marker_path, ec);
                }
            }
        };

        // --- Launch pipeline: bridge thread + execution worker pool + compilation threads ---

        // Determine number of execution threads for test execution parallelization
        unsigned int num_exec_threads;
        if (opts.test_threads > 0) {
            num_exec_threads = static_cast<unsigned int>(opts.test_threads);
        } else {
            num_exec_threads = 4u;
        }
        // Cap at reasonable maximum
        num_exec_threads = std::min(num_exec_threads, 32u);

        // Start bridge thread: transfers compiled suites from ready_queue to exec_queue
        std::thread bridge_thread(bridge_worker);

        // Start execution worker pool: N threads process exec_queue in parallel
        // Note: exec workers should ideally have a 128 MB stack like the old exec_thread
        // for OpenSSL DH/RSA operations, but std::thread doesn't support custom stack size.
        // If stack overflow issues occur in parallel mode, replace with NativeThread.
        std::vector<std::thread> exec_threads;
#ifdef _WIN32
        for (unsigned int t = 0; t < num_exec_threads; ++t) {
            exec_threads.emplace_back([&]() {
                // Wrapper to capture the lambda with custom stack size on Windows
                exec_worker();
            });
            // Note: Windows NativeThread would be needed for custom stack size,
            // but std::thread is simpler and should work for most cases.
            // If stack overflow issues occur in parallel mode, replace with NativeThread.
        }
#else
        // On Linux/macOS, default stack (typically 8 MB) is usually sufficient.
        for (unsigned int t = 0; t < num_exec_threads; ++t) {
            exec_threads.emplace_back(exec_worker);
        }
#endif

        if (suites_to_compile.empty()) {
            if (!opts.quiet && !suites_fully_cached.empty()) {
                TML_LOG_DEBUG("test", "All " << suites_fully_cached.size()
                                             << " suites cached, skipping compilation");
            }
            total_suites_to_run = 0;
            all_compiled.store(true);
            ready_cv.notify_one();
        } else {
            // Parallel suite compilation (3 threads).
            // Global state is thread-safe: GlobalASTCache/GlobalLibraryIRCache
            // use shared_mutex, LLVM target init uses std::once_flag,
            // meta preload uses std::call_once, TypeEnv/ModuleRegistry are
            // per-thread.
            unsigned int num_compile_threads;

            // NOTE: Coverage mode uses LLVM profiling which has thread-safety issues
            // with concurrent test compilation. Force single-threaded compilation during
            // coverage to avoid race conditions and deadlocks.
            if (opts.coverage) {
                num_compile_threads = 1;
                if (!opts.quiet) {
                    TML_LOG_INFO("test", "Coverage mode: using single-threaded compilation");
                }
            } else if (opts.test_threads > 0) {
                num_compile_threads = static_cast<unsigned int>(opts.test_threads);
            } else {
                unsigned int hw = std::thread::hardware_concurrency();
                if (hw == 0)
                    hw = 8;
                // Suite mode: keep conservative to allow per-suite Phase 1 parallelism
                // Individual mode: use half the cores for DLL-level parallelism
                num_compile_threads = opts.suite_mode ? 3u : std::max(4u, hw / 2);
            }

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

            total_suites_to_run = jobs.size();

            std::atomic<size_t> next_job{0};

            auto compile_worker = [&]() {
                while (true) {
                    if (fail_fast_triggered.load())
                        break;

                    size_t job_idx = next_job.fetch_add(1);
                    if (job_idx >= jobs.size())
                        break;

                    auto& job = jobs[job_idx];

                    // Always log which files are being compiled (even without --verbose)
                    std::string file_list;
                    for (size_t fi = 0; fi < job.suite.tests.size(); ++fi) {
                        if (fi > 0)
                            file_list += ", ";
                        file_list += fs::path(job.suite.tests[fi].file_path).filename().string();
                    }
                    TML_LOG_INFO("test", "[COMPILE] Starting: " << file_list);

                    if (!opts.quiet && opts.verbose) {
                        TML_LOG_DEBUG("test", "Compiling suite: " << job.suite.name << " ("
                                                                  << job.suite.tests.size()
                                                                  << " tests)");
                    }

                    set_crash_context("compiling", job.suite.name.c_str(), nullptr, nullptr);

                    job.result = compile_test_suite(job.suite, opts.verbose, opts.no_cache,
                                                    opts.backend, opts.features);

                    // Log completion of compilation
                    if (job.result.success) {
                        TML_LOG_INFO("test", "[COMPILE] Finished: " << file_list);
                    } else {
                        TML_LOG_ERROR("test", "[COMPILE] FAILED: " << file_list);
                    }
                    job.compiled = true;

                    if (job.result.success) {
                        // Push to ready queue for immediate execution
                        job.suite.dll_path = job.result.dll_path;
                        {
                            std::lock_guard<std::mutex> lock(ready_mutex);
                            ready_queue.push_back({std::move(job.suite), job.result.dll_path});
                        }
                        ready_cv.notify_one();
                    } else {
                        // Report compilation error immediately
                        std::string failed_file = job.result.failed_test;
                        if (failed_file.empty() && !job.suite.tests.empty()) {
                            failed_file = job.suite.tests[0].file_path;
                        }
                        std::string failed_name = failed_file.empty()
                                                      ? job.suite.name
                                                      : fs::path(failed_file).stem().string();

                        TestResult error_result;
                        error_result.file_path = failed_file;
                        error_result.test_name = failed_name;
                        error_result.group = job.suite.group;
                        error_result.passed = false;
                        error_result.compilation_error = true;
                        error_result.exit_code = EXIT_COMPILATION_ERROR;
                        error_result.error_message =
                            "COMPILATION FAILED\n" + job.result.error_message;

                        collector.add(std::move(error_result));

                        TML_LOG_ERROR("build", "COMPILATION FAILED suite="
                                                   << job.suite.name << " file=" << failed_file
                                                   << " error=" << job.result.error_message);
                    }
                }
            };

            if (!opts.quiet) {
                TML_LOG_DEBUG("test", "Compiling " << jobs.size() << " suites with "
                                                   << num_compile_threads << " threads...");
            }

            std::vector<std::thread> compile_threads;
            for (unsigned int t = 0; t < std::min(num_compile_threads, (unsigned int)jobs.size());
                 ++t) {
                compile_threads.emplace_back(compile_worker);
            }
            for (auto& t : compile_threads) {
                t.join();
            }

            // Signal bridge thread that all compilation is done
            all_compiled.store(true);
            ready_cv.notify_all();
        }

        // Wait for all pipeline threads to finish
        bridge_thread.join();
        // exec_enqueue_done is now true, exec workers will finish after processing remaining queue

        for (auto& t : exec_threads) {
            t.join();
        }

        // Check if any execution worker crashed
        if (exec_worker_crashed.load()) {
            TML_LOG_ERROR("test", "\n\n[PANIC] Test execution worker crashed!\n"
                                  "  File: "
                                      << crashed_test_file
                                      << "\n"
                                         "  Output: "
                                      << crashed_test_output << "\n");
            // Continue to report other results, but exit with error
            // (will be caught at the end of run_tests_suite_mode)
        }

        if (opts.profile) {
            collector.profile_stats.add(
                "pipeline_total",
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - pipeline_start)
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
                // Make a thread-safe copy of covered functions before reading
                std::set<std::string> covered_functions_copy;
                {
                    std::lock_guard<std::mutex> lock(coverage_mutex);
                    covered_functions_copy = all_covered_functions;
                }

                // Generate coverage report (even with failures — partial data is valuable)
                print_library_coverage_report(covered_functions_copy, c, test_stats);

                // Write HTML report with proper library coverage data ONLY if:
                // 1. Coverage is not zero
                // 2. Coverage PERCENTAGE is not regressing from previous report
                if (!CompilerOptions::coverage_output.empty()) {
                    int current_covered = static_cast<int>(covered_functions_copy.size());

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
                        write_library_coverage_html(covered_functions_copy, tmp_output, test_stats);

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

        // Check if any execution worker crashed
        if (exec_worker_crashed.load()) {
            return 1; // Exit with error due to worker crash
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
