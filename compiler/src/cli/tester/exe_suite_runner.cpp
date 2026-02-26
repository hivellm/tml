TML_MODULE("test")

//! # EXE-Based Suite Runner (Go-Style)
//!
//! Top-level orchestration for the EXE-based test execution system.
//! Equivalent to `run_tests_suite_mode()` but uses subprocess execution
//! instead of DLL loading.
//!
//! ## Flow
//!
//! ```text
//! discover tests → group into suites → compile EXEs → run 1 subprocess per suite → report
//! ```
//!
//! ## Optimization: --run-all mode
//!
//! Instead of spawning 1 CreateProcess per test file (~3,632 spawns at ~16ms each),
//! we spawn 1 subprocess per suite with `--run-all` (~454 spawns). The subprocess
//! runs all tests sequentially and prints structured TML_RESULT lines to stdout.
//! This reduces subprocess overhead from ~58s to ~7s.

#include "cli/tester/exe_test_runner.hpp"
#include "cli/tester/test_cache.hpp"
#include "log/log.hpp"
#include "tester_internal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace tml::cli::tester {

// ============================================================================
// Coverage Data Structures and Helpers
// ============================================================================

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

int run_tests_exe_mode(const std::vector<std::string>& test_files, const TestOptions& opts,
                       TestResultCollector& collector, const ColorOutput& c) {
    using Clock = std::chrono::high_resolution_clock;

    // Test cache for skipping unchanged tests
    TestCacheManager test_cache;
    fs::path cache_file = fs::path("build") / "debug" / ".test-cache.json";
    fs::path run_cache_dir = fs::path("build") / "debug" / ".run-cache";
    bool cache_loaded = false;
    std::atomic<int> skipped_count{0};

    bool should_update_cache = !opts.no_cache && opts.patterns.empty();

    if (!opts.no_cache) {
        cache_loaded = test_cache.load(cache_file.string());
        if (!cache_loaded && TestCacheManager::has_temp_backup()) {
            if (TestCacheManager::restore_from_temp(cache_file.string(), run_cache_dir.string())) {
                cache_loaded = test_cache.load(cache_file.string());
                if (cache_loaded && opts.verbose) {
                    TML_LOG_DEBUG("test", "[exe] Cache restored from backup");
                }
            }
        }
        if (opts.verbose && cache_loaded) {
            auto stats = test_cache.get_stats();
            TML_LOG_DEBUG("test",
                          "[exe] Loaded test cache with " << stats.total_entries << " entries");
        }
    }

    try {
        // Group test files into suites
        auto phase_start = Clock::now();
        auto suites = group_tests_into_suites(test_files);
        if (opts.profile) {
            collector.profile_stats.add(
                "exe.group_suites",
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start)
                    .count());
        }

        if (!opts.quiet) {
            TML_LOG_DEBUG("test", "[exe] Grouped into " << suites.size() << " test suite"
                                                        << (suites.size() != 1 ? "s" : ""));
        }

        // Cache file hashes
        std::map<std::string, std::string> file_hash_cache;

        // Check which suites can be entirely skipped
        std::vector<TestSuite> suites_to_compile;
        std::vector<TestSuite> suites_fully_cached;

        if (cache_loaded) {
            for (auto& suite : suites) {
                bool all_cached = true;
                for (const auto& test_info : suite.tests) {
                    if (!test_cache.can_skip(test_info.file_path)) {
                        all_cached = false;
                        break;
                    }
                    auto cached_info = test_cache.get_cached_info(test_info.file_path);
                    if (!cached_info) {
                        all_cached = false;
                        break;
                    }
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

        // Process fully cached suites
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
                TML_LOG_DEBUG("test", "[exe] Suite fully cached, skipped: " << suite.name);
            }
        }

        // Compile remaining suites to EXEs
        struct CompiledSuite {
            TestSuite suite;
            std::string exe_path;
        };
        std::vector<CompiledSuite> compiled_suites;
        std::atomic<bool> fail_fast_triggered{false};

        if (suites_to_compile.empty()) {
            if (!opts.quiet && !suites_fully_cached.empty()) {
                TML_LOG_DEBUG("test", "[exe] All " << suites_fully_cached.size()
                                                   << " suites cached, skipping compilation");
            }
        } else {
            // Compile suites sequentially (inner phases parallelize)
            if (!opts.quiet) {
                TML_LOG_DEBUG("test", "[exe] Compiling " << suites_to_compile.size()
                                                         << " suites to EXEs...");
            }

            phase_start = Clock::now();

            for (auto& suite : suites_to_compile) {
                // Break immediately if fail_fast was triggered by previous compilation failure
                if (fail_fast_triggered.load()) {
                    TML_LOG_DEBUG("test", "[exe] Stopping compilation due to fail_fast");
                    break;
                }

                if (!opts.quiet && opts.verbose) {
                    TML_LOG_DEBUG("test", "[exe] Compiling suite: " << suite.name << " ("
                                                                    << suite.tests.size()
                                                                    << " tests)");
                }

                auto compile_result = compile_test_suite_exe(suite, opts.verbose, opts.no_cache);

                if (!compile_result.success) {
                    // Report compilation error but continue with other suites
                    TestResult error_result;
                    error_result.file_path = compile_result.failed_test;
                    error_result.test_name = fs::path(compile_result.failed_test).stem().string();
                    error_result.group = suite.group;
                    error_result.passed = false;
                    error_result.compilation_error = true;
                    error_result.exit_code = 99;
                    error_result.error_message =
                        "COMPILATION FAILED\n" + compile_result.error_message;

                    collector.add(std::move(error_result));

                    TML_LOG_ERROR("build", "[exe] COMPILATION FAILED suite="
                                               << suite.name
                                               << " file=" << compile_result.failed_test
                                               << " error=" << compile_result.error_message);

                    // Trigger fail_fast on compilation failure
                    if (opts.fail_fast) {
                        fail_fast_triggered.store(true);
                        TML_LOG_ERROR("test", "fail_fast triggered due to compilation failure");
                    }
                    continue;
                }

                compiled_suites.push_back({std::move(suite), compile_result.exe_path});
            }

            if (opts.profile) {
                collector.profile_stats.add("exe.compile",
                                            std::chrono::duration_cast<std::chrono::microseconds>(
                                                Clock::now() - phase_start)
                                                .count());
            }
        }

        // ======================================================================
        // Run tests via --run-all subprocess (1 process per suite)
        // ======================================================================

        std::mutex cache_mutex;

        // Coverage tracking for subprocess mode
        std::set<std::string> all_covered_functions;
        std::mutex coverage_mutex;

        // Determine execution thread count
        unsigned int hw_threads = std::thread::hardware_concurrency();
        if (hw_threads == 0)
            hw_threads = 8;
        // Use half of available threads for subprocess execution, max 8 threads
        unsigned int num_exec_threads = std::clamp(hw_threads / 2, 2u, 8u);
        num_exec_threads =
            std::min(num_exec_threads, static_cast<unsigned int>(compiled_suites.size()));

        std::atomic<size_t> suite_index{0};
        std::vector<AsyncSubprocessHandle> pending_suites;
        std::vector<size_t> pending_suite_indices;
        std::mutex pending_mutex;
        const size_t max_concurrent = std::min(num_exec_threads * 2, 16u);

        auto suite_worker = [&]() {
            while (true) {
                if (fail_fast_triggered.load()) {
                    return;
                }

                // Check ALL pending subprocesses for completion (non-blocking poll)
                {
                    std::lock_guard<std::mutex> lock(pending_mutex);
                    for (int i = (int)pending_suites.size() - 1; i >= 0; --i) {
                        if (subprocess_is_done(pending_suites[i])) {
                            // This one is done, collect its result
                            SuiteSubprocessResult suite_result =
                                wait_for_subprocess(pending_suites[i]);
                            size_t completed_idx = pending_suite_indices[i];
                            pending_suites.erase(pending_suites.begin() + i);
                            pending_suite_indices.erase(pending_suite_indices.begin() + i);

                            auto& [suite, exe_path] = compiled_suites[completed_idx];
                            TML_LOG_DEBUG("test", "[exe] Suite completed: " << suite.name);

                            // Process result (cache, fail_fast check, coverage collection)
                            // This was previously in lines 344+, moved to after each collection
                            // [Collection and processing code continues below at the end of this
                            // block] For now, mark that we need to process this result We'll break
                            // and process one at a time to keep locks minimal
                            break;
                        }
                    }
                }

                // Launch new subprocess if we have capacity
                {
                    std::lock_guard<std::mutex> lock(pending_mutex);
                    if (pending_suites.size() < max_concurrent) {
                        size_t idx = suite_index.fetch_add(1);
                        if (idx >= compiled_suites.size()) {
                            // No more suites to launch, wait for pending to finish
                            if (pending_suites.empty()) {
                                return; // All done
                            }
                            // Continue polling pending subprocesses
                        } else {
                            auto& [suite, exe_path] = compiled_suites[idx];
                            auto handle = launch_subprocess_async(
                                exe_path, static_cast<int>(suite.tests.size()),
                                opts.timeout_seconds > 0
                                    ? opts.timeout_seconds * static_cast<int>(suite.tests.size())
                                    : 300,
                                suite.name, opts);
                            pending_suites.push_back(handle);
                            pending_suite_indices.push_back(idx);
                            continue; // Launch more if we have capacity
                        }
                    }
                }

                // Small sleep to avoid busy-waiting (use 1ms instead of 10ms for responsiveness)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                // Collect results from ANY completed subprocess
                SuiteSubprocessResult suite_result;
                size_t completed_idx = 0;
                bool found_completed = false;
                {
                    std::lock_guard<std::mutex> lock(pending_mutex);
                    for (size_t i = 0; i < pending_suites.size(); ++i) {
                        if (subprocess_is_done(pending_suites[i])) {
                            suite_result = wait_for_subprocess(pending_suites[i]);
                            completed_idx = pending_suite_indices[i];
                            pending_suites.erase(pending_suites.begin() + i);
                            pending_suite_indices.erase(pending_suite_indices.begin() + i);
                            found_completed = true;
                            break;
                        }
                    }
                }

                if (!found_completed) {
                    continue; // No subprocess completed yet, loop and check again
                }

                auto& [suite, exe_path] = compiled_suites[completed_idx];

                TML_LOG_DEBUG("test", "[exe] Running suite via --run-all: " << suite.name << " ("
                                                                            << suite.tests.size()
                                                                            << " test files)");

                // Check per-file cache — skip individual cached files
                // Build a map of which tests actually need to run
                std::vector<bool> needs_run(suite.tests.size(), true);
                std::vector<std::string> file_hashes(suite.tests.size());

                for (size_t i = 0; i < suite.tests.size(); ++i) {
                    const auto& test_info = suite.tests[i];

                    // Compute file hash
                    auto it = file_hash_cache.find(test_info.file_path);
                    if (it != file_hash_cache.end()) {
                        file_hashes[i] = it->second;
                    } else {
                        file_hashes[i] = TestCacheManager::compute_file_hash(test_info.file_path);
                    }

                    // Check cache
                    if (cache_loaded) {
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        if (test_cache.can_skip(test_info.file_path)) {
                            auto cached_info = test_cache.get_cached_info(test_info.file_path);
                            if (cached_info && cached_info->sha512 == file_hashes[i]) {
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
                                needs_run[i] = false;

                                TML_LOG_DEBUG("test",
                                              "[exe] Skipped (cached): " << test_info.test_name);
                            }
                        }
                    }
                }

                // Check if any tests actually need to run
                bool any_need_run = false;
                for (bool b : needs_run) {
                    if (b) {
                        any_need_run = true;
                        break;
                    }
                }

                if (!any_need_run) {
                    TML_LOG_DEBUG("test", "[exe] Suite fully cached: " << suite.name);
                    if (should_update_cache) {
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        fs::create_directories(cache_file.parent_path());
                        test_cache.save(cache_file.string());
                    }
                    continue;
                }

                // suite_result was already assigned from wait_for_subprocess above
                // The duration was already calculated there
                auto suite_duration_us = suite_result.total_duration_us;
                int64_t per_test_ms =
                    suite.tests.size() > 0
                        ? (suite_duration_us / 1000) / static_cast<int64_t>(suite.tests.size())
                        : 0;

                if (opts.profile) {
                    collector.profile_stats.add("exe.suite_run", suite_duration_us);
                }

                if (suite_result.timed_out) {
                    // Entire suite timed out — mark all unfinished tests as failed
                    for (size_t i = 0; i < suite.tests.size(); ++i) {
                        if (!needs_run[i])
                            continue;
                        const auto& test_info = suite.tests[i];
                        TestResult result;
                        result.file_path = test_info.file_path;
                        result.test_name = test_info.test_name;
                        result.group = suite.group;
                        result.test_count = test_info.test_count;
                        result.passed = false;
                        result.timeout = true;
                        result.exit_code = -1;
                        result.duration_ms = per_test_ms;
                        result.error_message = "\n  TIMEOUT: " + test_info.test_name +
                                               "\n  Suite: " + suite.name + "\n";
                        if (opts.profile) {
                            collector.profile_stats.total_tests++;
                        }
                        collector.add(std::move(result));
                    }
                    if (opts.fail_fast) {
                        fail_fast_triggered.store(true);
                    }
                } else if (!suite_result.process_ok) {
                    // Process failed to launch — mark all as failed
                    for (size_t i = 0; i < suite.tests.size(); ++i) {
                        if (!needs_run[i])
                            continue;
                        const auto& test_info = suite.tests[i];
                        TestResult result;
                        result.file_path = test_info.file_path;
                        result.test_name = test_info.test_name;
                        result.group = suite.group;
                        result.test_count = test_info.test_count;
                        result.passed = false;
                        result.exit_code = -1;
                        result.duration_ms = 0;
                        result.error_message = "\n  LAUNCH FAILED: " + test_info.test_name +
                                               "\n  Stderr: " + suite_result.stderr_output + "\n";
                        if (opts.profile) {
                            collector.profile_stats.total_tests++;
                        }
                        collector.add(std::move(result));
                    }
                } else {
                    // Process completed — map outcomes to test results
                    // Build a lookup by test_index
                    std::map<int, const SuiteSubprocessResult::TestOutcome*> outcome_map;
                    for (const auto& o : suite_result.outcomes) {
                        outcome_map[o.test_index] = &o;
                    }

                    for (size_t i = 0; i < suite.tests.size(); ++i) {
                        if (!needs_run[i])
                            continue;

                        const auto& test_info = suite.tests[i];
                        auto it = outcome_map.find(static_cast<int>(i));

                        TestResult result;
                        result.file_path = test_info.file_path;
                        result.test_name = test_info.test_name;
                        result.group = suite.group;
                        result.test_count = test_info.test_count;
                        result.duration_ms = per_test_ms;

                        if (it != outcome_map.end()) {
                            result.passed = it->second->passed;
                            result.exit_code = it->second->exit_code;
                        } else {
                            // Missing result — process crashed before this test ran
                            result.passed = false;
                            result.exit_code = -1;
                            result.error_message = "\n  CRASHED: " + test_info.test_name +
                                                   "\n  Suite " + suite.name +
                                                   " crashed before this test completed\n";
                        }

                        if (!result.passed && result.error_message.empty()) {
                            result.error_message = "\n  FAILED: " + test_info.test_name;
                            result.error_message += "\n  File:   " + test_info.file_path;
                            result.error_message +=
                                "\n  Exit:   " + std::to_string(result.exit_code);
                            if (!suite_result.stderr_output.empty()) {
                                result.error_message += "\n  Stderr: " + suite_result.stderr_output;
                            }
                            result.error_message += "\n";

                            TML_LOG_ERROR("test",
                                          "[exe] FAILED test=" << test_info.test_name
                                                               << " file=" << test_info.file_path
                                                               << " exit=" << result.exit_code);
                        }

                        if (opts.profile) {
                            collector.profile_stats.total_tests++;
                        }

                        // Update cache
                        if (should_update_cache) {
                            std::lock_guard<std::mutex> lock(cache_mutex);
                            std::vector<std::string> test_functions;
                            test_cache.update(
                                test_info.file_path, file_hashes[i], suite.name, test_functions,
                                result.passed ? CachedTestStatus::Pass : CachedTestStatus::Fail,
                                result.duration_ms, {}, opts.coverage, opts.profile);
                        }

                        collector.add(std::move(result));

                        if (opts.fail_fast && !result.passed) {
                            fail_fast_triggered.store(true);
                            TML_LOG_WARN("test", "[exe] Test failed, stopping due to --fail-fast");
                            break;
                        }
                    }
                }

                // Incremental cache save after each suite
                if (should_update_cache) {
                    std::lock_guard<std::mutex> lock(cache_mutex);
                    fs::create_directories(cache_file.parent_path());
                    test_cache.save(cache_file.string());
                }

                // Collect coverage data from subprocess temporary file
                if (opts.coverage && !suite.name.empty()) {
                    fs::path cov_file = fs::path("build/coverage") / ("cov_" + suite.name + ".txt");
                    if (fs::exists(cov_file)) {
                        std::ifstream in(cov_file);
                        std::string func_name;
                        {
                            std::lock_guard<std::mutex> lock(coverage_mutex);
                            while (std::getline(in, func_name)) {
                                if (!func_name.empty()) {
                                    all_covered_functions.insert(func_name);
                                }
                            }
                        }
                        in.close();
                        // Clean up the temporary file
                        try {
                            fs::remove(cov_file);
                        } catch (...) {
                            // Ignore cleanup errors
                        }
                        TML_LOG_DEBUG("test",
                                      "[exe] Collected coverage from: " << cov_file.string());
                    }
                }
            }
        };

        // Launch parallel execution threads
        if (!compiled_suites.empty()) {
            if (!opts.quiet) {
                TML_LOG_DEBUG("test", "[exe] Running " << compiled_suites.size()
                                                       << " suites (--run-all mode) with "
                                                       << num_exec_threads << " threads...");
            }

            phase_start = Clock::now();
            std::vector<std::thread> exec_threads;
            for (unsigned int t = 0;
                 t < std::min(num_exec_threads, (unsigned int)compiled_suites.size()); ++t) {
                exec_threads.emplace_back(suite_worker);
            }
            for (auto& t : exec_threads) {
                t.join();
            }

            if (opts.profile) {
                collector.profile_stats.add("exe.parallel_execute",
                                            std::chrono::duration_cast<std::chrono::microseconds>(
                                                Clock::now() - phase_start)
                                                .count());
            }
        }

        // Check fail-fast
        if (fail_fast_triggered.load()) {
            if (should_update_cache) {
                fs::create_directories(cache_file.parent_path());
                test_cache.save(cache_file.string());
            }
            return 1;
        }

        // Save cache
        if (should_update_cache) {
            fs::create_directories(cache_file.parent_path());
            if (test_cache.save(cache_file.string())) {
                if (opts.verbose) {
                    auto stats = test_cache.get_stats();
                    TML_LOG_DEBUG("test", "[exe] Saved test cache with " << stats.total_entries
                                                                         << " entries");
                }
                TestCacheManager::backup_to_temp(cache_file.string(), run_cache_dir.string());
            }
        }

        // Report skipped tests
        int skipped = skipped_count.load();
        if (skipped > 0 && !opts.quiet) {
            TML_LOG_DEBUG("test", "[exe] Skipped " << skipped << " cached test"
                                                   << (skipped != 1 ? "s" : "") << " (unchanged)");
        }

        // Generate coverage report if coverage was enabled
        if (opts.coverage) {
            // Build TestRunStats from collector results
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
            std::sort(test_stats.suites.begin(), test_stats.suites.end(),
                      [](const SuiteStats& a, const SuiteStats& b) {
                          return a.test_count > b.test_count;
                      });

            // Make thread-safe copy of covered functions
            std::set<std::string> covered_functions_copy;
            {
                std::lock_guard<std::mutex> lock(coverage_mutex);
                covered_functions_copy = all_covered_functions;
            }

            // Generate coverage report
            print_library_coverage_report(covered_functions_copy, c, test_stats);

            // Write HTML report if output path is configured
            if (!CompilerOptions::coverage_output.empty()) {
                int current_covered = static_cast<int>(covered_functions_copy.size());

                // Never update with zero coverage - something went wrong
                if (current_covered == 0) {
                    TML_LOG_FATAL("test", "========================================================"
                                          "======");
                    TML_LOG_FATAL("test", "  COVERAGE ABORTED: Zero functions tracked");
                    TML_LOG_FATAL("test", "  Tests ran but no coverage data was collected.");
                    TML_LOG_FATAL("test", "  This indicates a bug in coverage instrumentation.");
                    TML_LOG_FATAL("test", "  HTML/JSON files will NOT be generated.");
                    TML_LOG_FATAL("test", "========================================================"
                                          "======");
                } else {
                    auto previous = get_previous_coverage(CompilerOptions::coverage_output);

                    // Use the previous total as reference for calculating current percentage
                    bool should_update = true;
                    double current_percent = 0.0;

                    if (previous.valid && previous.total > 0) {
                        current_percent = (100.0 * current_covered) / previous.total;
                        should_update = current_percent >= previous.percent;
                    }

                    // Always write to temp files
                    std::string tmp_output = CompilerOptions::coverage_output + ".tmp";
                    write_library_coverage_html(covered_functions_copy, tmp_output, test_stats);

                    // Handle JSON file renaming
                    std::string tmp_json = fs::path(tmp_output).replace_extension(".json").string();
                    std::string final_json = fs::path(CompilerOptions::coverage_output)
                                                 .replace_extension(".json")
                                                 .string();

                    if (should_update) {
                        bool html_ok = fs::exists(tmp_output) && fs::file_size(tmp_output) > 0;
                        bool json_ok = fs::exists(tmp_json) && fs::file_size(tmp_json) > 0;

                        if (html_ok) {
                            fs::rename(tmp_output, CompilerOptions::coverage_output);
                            if (!opts.quiet) {
                                TML_LOG_DEBUG(
                                    "test", "[exe] Updated: " << CompilerOptions::coverage_output);
                            }
                        }

                        if (json_ok) {
                            fs::rename(tmp_json, final_json);
                            if (!opts.quiet) {
                                TML_LOG_DEBUG("test", "[exe] Updated: " << final_json);
                            }
                        }
                    } else {
                        // Regression detected — keep previous report
                        if (fs::exists(tmp_output))
                            fs::remove(tmp_output);
                        if (fs::exists(tmp_json))
                            fs::remove(tmp_json);
                        if (!opts.quiet) {
                            TML_LOG_WARN("test",
                                         "[exe] Coverage regressed: " << current_percent << "% vs "
                                                                      << previous.percent
                                                                      << "% (previous). "
                                                                         "Keeping old report.");
                        }
                    }
                }
            }
        }

        return 0;

    } catch (const std::exception& e) {
        if (should_update_cache) {
            try {
                fs::create_directories(cache_file.parent_path());
                test_cache.save(cache_file.string());
            } catch (...) {}
        }
        TML_LOG_FATAL("test", "[exe] Exception in run_tests_exe_mode: " << e.what());
        return 1;
    } catch (...) {
        if (should_update_cache) {
            try {
                fs::create_directories(cache_file.parent_path());
                test_cache.save(cache_file.string());
            } catch (...) {}
        }
        TML_LOG_FATAL("test", "[exe] Unknown exception in run_tests_exe_mode");
        return 1;
    }
}

} // namespace tml::cli::tester
