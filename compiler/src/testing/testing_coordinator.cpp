TML_MODULE("test")

//! # Test Coordinator (v3 — fully independent)
//!
//! Central orchestrator for the TML test system. Uses only modules from
//! compiler/src/testing/ and compiler/include/testing/. Zero dependency
//! on the old test system.
//!
//! Pipeline:
//!   testing::discover_tests() → testing::group_into_suites()
//!   → testing::compile_suites_parallel() → Process::launch() + NDJSON parse
//!   → testing::print_coverage_report() → TestRunResult

#include "testing/testing_coordinator.hpp"

#include "common.hpp"
#include "log/log.hpp"
#include "testing/testing_compile.hpp"
#include "testing/testing_coverage.hpp"
#include "testing/testing_discovery.hpp"
#include "testing/testing_dispatcher_gen.hpp"
#include "testing/testing_process.hpp"
#include "testing/testing_protocol.hpp"
#include "testing/testing_test_cache.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace tml::testing {

using Clock = std::chrono::high_resolution_clock;

// ============================================================================
// Internal helpers
// ============================================================================

/// Normalize path slashes for cross-platform matching.
static std::string normalize_slashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

/// Filter test files by pattern (substring match, slash-normalized).
static std::vector<TestFileInfo> filter_by_patterns(const std::vector<TestFileInfo>& files,
                                                    const std::vector<std::string>& patterns) {
    if (patterns.empty())
        return files;

    std::vector<TestFileInfo> filtered;
    for (const auto& file : files) {
        std::string norm_file = normalize_slashes(file.file_path);
        for (const auto& pattern : patterns) {
            std::string norm_pattern = normalize_slashes(pattern);
            if (norm_file.find(norm_pattern) != std::string::npos) {
                filtered.push_back(file);
                break;
            }
        }
    }
    return filtered;
}

/// Convert suite filters (e.g. "core/str") to file path patterns.
static std::vector<std::string>
suite_filters_to_patterns(const std::vector<std::string>& suite_filters) {
    std::vector<std::string> patterns;
    for (const auto& filter : suite_filters) {
        auto slash_pos = filter.find('/');
        if (slash_pos != std::string::npos) {
            std::string lib_name = filter.substr(0, slash_pos);
            std::string module_name = filter.substr(slash_pos + 1);

            if (lib_name == "compiler") {
                patterns.push_back("compiler/tests/" + module_name);
            } else {
                patterns.push_back("lib/" + lib_name + "/tests/" + module_name);
            }
        } else {
            patterns.push_back(filter);
        }
    }
    return patterns;
}

/// Parse NDJSON stdout into a SuiteRunResult.
static SuiteRunResult parse_ndjson_output(const std::string& stdout_data, const Suite& suite,
                                          int64_t exec_time_us) {

    SuiteRunResult result;
    result.name = suite.name;
    result.group = suite.group;
    result.test_count = static_cast<int>(suite.tests.size());
    result.exec_time_us = exec_time_us;

    // Pre-fill test results from suite metadata
    result.tests.resize(suite.tests.size());
    for (int i = 0; i < static_cast<int>(suite.tests.size()); ++i) {
        result.tests[i].index = i;
        result.tests[i].name = suite.tests[i].test_name;
        result.tests[i].file = suite.tests[i].file_path;
    }

    // Parse each NDJSON line
    std::istringstream stream(stdout_data);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        auto parsed = parse_json_event(line);
        if (!parsed.ok)
            continue;

        std::visit(
            [&](auto&& ev) {
                using T = std::decay_t<decltype(ev)>;
                if constexpr (std::is_same_v<T, TestPassEvent>) {
                    if (ev.index >= 0 && ev.index < static_cast<int>(result.tests.size())) {
                        result.tests[ev.index].passed = true;
                        result.tests[ev.index].duration_us = ev.duration_us;
                        result.passed++;
                    }
                } else if constexpr (std::is_same_v<T, TestFailEvent>) {
                    if (ev.index >= 0 && ev.index < static_cast<int>(result.tests.size())) {
                        result.tests[ev.index].passed = false;
                        result.tests[ev.index].exit_code = ev.exit_code;
                        result.tests[ev.index].error = ev.error;
                        result.tests[ev.index].duration_us = ev.duration_us;
                        result.failed++;
                    }
                } else if constexpr (std::is_same_v<T, TestCrashEvent>) {
                    if (ev.index >= 0 && ev.index < static_cast<int>(result.tests.size())) {
                        result.tests[ev.index].passed = false;
                        result.tests[ev.index].error = "CRASH: " + ev.signal;
                        result.tests[ev.index].duration_us = ev.duration_us;
                        result.crashed++;
                    }
                } else if constexpr (std::is_same_v<T, TestTimeoutEvent>) {
                    if (ev.index >= 0 && ev.index < static_cast<int>(result.tests.size())) {
                        result.tests[ev.index].passed = false;
                        result.tests[ev.index].error = "TIMEOUT";
                    }
                }
            },
            parsed.event);
    }

    return result;
}

// ============================================================================
// Execution phase
// ============================================================================

static std::vector<SuiteRunResult>
execute_suites_parallel(const std::vector<Suite>& suites,
                        const std::vector<CompileResult>& compile_results, const TestConfig& config,
                        std::atomic<bool>& should_stop,
                        std::set<std::string>* out_covered = nullptr) {

    std::mutex cov_mutex;
    std::vector<SuiteRunResult> results(suites.size());

    int max_concurrent = config.exec_concurrent;
    if (max_concurrent <= 0) {
        max_concurrent = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    }

    // Build list of suites that compiled successfully
    struct ExecWork {
        int suite_index;
        std::string exe_path;
    };
    std::vector<ExecWork> pending;
    for (int i = 0; i < static_cast<int>(compile_results.size()); ++i) {
        if (compile_results[i].success) {
            pending.push_back({i, compile_results[i].exe_path});
        } else {
            auto& r = results[i];
            r.name = suites[i].name;
            r.group = suites[i].group;
            r.test_count = static_cast<int>(suites[i].tests.size());
            r.compile_ok = false;
            r.compile_error = compile_results[i].error_message;
            r.compile_time_us = compile_results[i].compile_time_us;
        }
    }

    // Fill compile times for successful suites
    for (int i = 0; i < static_cast<int>(compile_results.size()); ++i) {
        if (compile_results[i].success) {
            results[i].compile_time_us = compile_results[i].compile_time_us;
        }
    }

    auto now_us = []() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   Clock::now().time_since_epoch())
            .count();
    };

    // Launch and poll subprocesses
    struct RunningProc {
        int suite_index;
        Process proc;
        std::string accumulated_stdout;
        std::string accumulated_stderr;
        int64_t start_us;
    };

    std::vector<RunningProc> running;
    size_t next_pending = 0;

    while (!should_stop.load(std::memory_order_relaxed)) {
        // Launch new processes up to max_concurrent
        while (static_cast<int>(running.size()) < max_concurrent && next_pending < pending.size() &&
               !should_stop.load(std::memory_order_relaxed)) {

            auto& work = pending[next_pending++];
            auto& suite = suites[work.suite_index];

            ProcessOptions opts;
            opts.exe_path = work.exe_path;
            opts.args = {"--run-all"};
            opts.timeout = std::chrono::seconds(config.timeout_seconds);

            // Coverage: tell subprocess to write covered functions to a file
            if (config.coverage && out_covered) {
                auto cov_dir = fs::current_path() / "build" / "coverage";
                std::error_code ec;
                fs::create_directories(cov_dir, ec);
                opts.env["TML_COVERAGE_FILE"] = (cov_dir / ("cov_" + suite.name + ".txt")).string();

                // Redirect LLVM profraw files to build/coverage/profraw/
                // to prevent default.profraw from landing in the project root
                auto profraw_dir = cov_dir / "profraw";
                fs::create_directories(profraw_dir, ec);
                opts.env["LLVM_PROFILE_FILE"] =
                    (profraw_dir / (suite.name + "-%p%m.profraw")).string();
            }

            TML_LOG_DEBUG("test", "Launching subprocess: " << suite.name);

            auto proc = Process::launch(opts);
            if (proc) {
                running.push_back({work.suite_index, std::move(*proc), "", "", now_us()});
            } else {
                auto& r = results[work.suite_index];
                r.name = suite.name;
                r.group = suite.group;
                r.test_count = static_cast<int>(suite.tests.size());
                r.compile_ok = true;
                r.compile_error = "Failed to launch subprocess";
                r.failed = r.test_count;
            }
        }

        if (running.empty())
            break;

        // Poll running processes
        bool any_done = false;
        for (auto it = running.begin(); it != running.end();) {
            auto chunk = it->proc.read_stdout();
            if (!chunk.empty())
                it->accumulated_stdout += chunk;
            auto err_chunk = it->proc.read_stderr();
            if (!err_chunk.empty())
                it->accumulated_stderr += err_chunk;

            if (it->proc.is_done()) {
                any_done = true;
                auto final_chunk = it->proc.read_stdout();
                if (!final_chunk.empty())
                    it->accumulated_stdout += final_chunk;
                auto final_err = it->proc.read_stderr();
                if (!final_err.empty())
                    it->accumulated_stderr += final_err;

                int64_t exec_us = now_us() - it->start_us;
                auto proc_result = it->proc.wait(std::chrono::milliseconds(100));
                // Merge any remaining stderr from wait()
                if (!proc_result.stderr_output.empty())
                    it->accumulated_stderr += proc_result.stderr_output;

                auto& suite = suites[it->suite_index];

                results[it->suite_index] =
                    parse_ndjson_output(it->accumulated_stdout, suite, exec_us);
                auto& sr = results[it->suite_index];
                sr.compile_time_us = compile_results[it->suite_index].compile_time_us;

                // Propagate per-file compile errors
                for (const auto& pfe : compile_results[it->suite_index].per_file_errors) {
                    sr.per_file_compile_errors.push_back({pfe.file_path, pfe.error});
                }

                // Propagate stderr for diagnostics
                if (!it->accumulated_stderr.empty()) {
                    sr.process_stderr = it->accumulated_stderr;
                }

                // When process crashed (non-zero exit, no NDJSON events parsed),
                // propagate stderr to all failed tests so the user sees WHY it failed.
                if (proc_result.exit_code != 0) {
                    for (auto& t : sr.tests) {
                        if (!t.passed && t.error.empty()) {
                            t.exit_code = proc_result.exit_code;
                            if (!it->accumulated_stderr.empty()) {
                                t.error = it->accumulated_stderr;
                            } else {
                                t.error = "Process exited with code " +
                                          std::to_string(proc_result.exit_code);
                            }
                        }
                    }
                    // If no tests were reported at all, this was a process crash
                    if (sr.passed == 0 && sr.failed == 0 && sr.crashed == 0) {
                        sr.crashed = sr.test_count;
                        sr.failed = 0;
                    }
                }

                if (proc_result.timed_out) {
                    sr.failed = sr.test_count;
                    for (auto& t : sr.tests) {
                        if (!t.passed && t.error.empty())
                            t.error = "TIMEOUT (suite-level)";
                    }
                }

                if (config.fail_fast && (sr.failed > 0 || sr.crashed > 0)) {
                    should_stop.store(true, std::memory_order_relaxed);
                }

                TML_LOG_DEBUG("test", "Suite done: " << suite.name << " (passed=" << sr.passed
                                                     << " failed=" << sr.failed
                                                     << " crashed=" << sr.crashed << ")");

                // Read coverage data from subprocess output file
                if (config.coverage && out_covered) {
                    auto cov_file =
                        fs::current_path() / "build" / "coverage" / ("cov_" + suite.name + ".txt");
                    if (fs::exists(cov_file)) {
                        std::ifstream in(cov_file);
                        std::string func_name;
                        std::lock_guard<std::mutex> lock(cov_mutex);
                        while (std::getline(in, func_name)) {
                            if (!func_name.empty() && func_name.back() == '\r')
                                func_name.pop_back();
                            if (!func_name.empty())
                                out_covered->insert(func_name);
                        }
                        in.close();
                        std::error_code ec;
                        fs::remove(cov_file, ec);
                    }
                }

                it = running.erase(it);
            } else {
                ++it;
            }
        }

        if (!any_done && !running.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // Kill remaining processes on fail-fast
    if (should_stop.load(std::memory_order_relaxed)) {
        for (auto& rp : running)
            rp.proc.kill();
    }

    return results;
}

// ============================================================================
// Entry point
// ============================================================================

TestRunResult run_tests(const TestConfig& config) {
    TestRunResult result;
    auto total_start = Clock::now();

    // Redirect LLVM profraw from the main process to build/coverage/profraw/
    // Without this, LLVM's profiling runtime writes default.profraw to the project root.
    if (config.coverage) {
        auto profraw_dir = fs::current_path() / "build" / "coverage" / "profraw";
        std::error_code ec;
        fs::create_directories(profraw_dir, ec);
        auto profraw_path = (profraw_dir / "compiler-%p%m.profraw").string();
#ifdef _WIN32
        _putenv_s("LLVM_PROFILE_FILE", profraw_path.c_str());
#else
        setenv("LLVM_PROFILE_FILE", profraw_path.c_str(), 1);
#endif
    }

    // 1. Discovery (independent)
    std::string root = config.root_dir;
    if (root.empty())
        root = fs::current_path().string();

    auto test_files = discover_tests(root);
    if (test_files.empty()) {
        TML_LOG_INFO("test", "No test files found");
        return result;
    }

    // 2. Suite filters → file path patterns
    auto all_patterns = config.patterns;
    if (!config.suite_filters.empty()) {
        auto suite_patterns = suite_filters_to_patterns(config.suite_filters);
        all_patterns.insert(all_patterns.end(), suite_patterns.begin(), suite_patterns.end());
    }

    // 3. Pattern filtering
    test_files = filter_by_patterns(test_files, all_patterns);
    if (test_files.empty()) {
        TML_LOG_INFO("test", "No tests matched the specified pattern(s)");
        return result;
    }

    TML_LOG_DEBUG("test", "[coordinator] After filtering: " << test_files.size() << " test files");

    // 4. Suite grouping (default max_per_suite=10; coverage uses 1)
    auto suites = group_into_suites(test_files, config.max_per_suite);
    TML_LOG_DEBUG("test", "[coordinator] After grouping: " << suites.size() << " suites from "
                                                           << test_files.size() << " files");
    if (suites.empty()) {
        TML_LOG_INFO("test", "No suites matched the specified filter(s)");
        return result;
    }

    // List suites mode
    if (config.list_suites) {
        for (const auto& suite : suites) {
            TML_LOG_INFO("test", suite.group << "/" << suite.name << " (" << suite.tests.size()
                                             << " tests)");
        }
        return result;
    }

    TML_LOG_INFO("test", "[coordinator] Running " << test_files.size() << " tests in "
                                                  << suites.size() << " suites");

    // 5. Cache: load and check
    TestResultCache cache;
    std::string compiler_hash;
    std::string flags_hash;

    bool use_cache = !config.no_cache;
    if (use_cache) {
        auto build_dir = fs::current_path() / "build" / "debug";
        auto cache_file = build_dir / ".new-test-cache.json";
        if (cache.load(cache_file.string())) {
            TML_LOG_INFO("test", "  [cache] Loaded " << cache.size() << " cached suites from "
                                                     << cache_file.string());
        } else {
            TML_LOG_INFO("test", "  [cache] No cache file at " << cache_file.string());
        }

        compiler_hash = TestResultCache::compute_compiler_hash();
        flags_hash = TestResultCache::compute_flags_hash(config);

        if (!cache.compiler_hash().empty() && cache.compiler_hash() != compiler_hash) {
            TML_LOG_DEBUG("test", "[cache] Compiler changed, invalidating cache");
            cache.clear();
        }
        cache.set_compiler_hash(compiler_hash);
    }

    // 5b. Partition suites into:
    //   - cached_results:    fully cached (passed last time, skip compile+run)
    //   - reuse_exe_suites:  source unchanged + exe on disk (skip compile, re-run)
    //   - uncached_suites:   need full compile+run
    std::vector<Suite> uncached_suites;
    std::vector<SuiteRunResult> cached_results;
    // Suites whose previous exe can be reused (source unchanged, but failed last time)
    std::vector<Suite> reuse_exe_suites;
    std::vector<std::string> reuse_exe_paths; // parallel to reuse_exe_suites

    if (use_cache) {
        for (auto& suite : suites) {
            std::vector<std::string> file_paths;
            file_paths.reserve(suite.tests.size());
            for (const auto& t : suite.tests)
                file_paths.push_back(t.file_path);
            auto source_hashes = TestResultCache::compute_source_hashes(file_paths);

            if (cache.is_cached(suite.name, source_hashes, flags_hash)) {
                const auto* entry = cache.get(suite.name);
                SuiteRunResult sr;
                sr.name = suite.name;
                sr.group = suite.group;
                sr.test_count = entry->test_count;
                sr.passed = entry->passed_count;
                sr.failed = entry->failed_count;
                sr.crashed = 0;
                sr.compile_ok = true;
                sr.cached = true;
                sr.exec_time_us = entry->duration_us;
                cached_results.push_back(std::move(sr));
                TML_LOG_INFO("test",
                             "  [cache] Suite " << suite.name << " is cached (passed, skipping)");
            } else if (auto exe = cache.get_reusable_exe(suite.name, source_hashes, flags_hash);
                       !exe.empty()) {
                // Source unchanged + exe exists: skip recompilation, just re-run
                TML_LOG_INFO("test", "  [cache] Suite " << suite.name
                                                        << " exe reusable (source unchanged, "
                                                           "skipping recompilation)");
                reuse_exe_paths.push_back(std::move(exe));
                reuse_exe_suites.push_back(std::move(suite));
            } else {
                uncached_suites.push_back(std::move(suite));
            }
        }

        if (!cached_results.empty() || !reuse_exe_suites.empty()) {
            TML_LOG_INFO("test", "[coordinator] " << cached_results.size() << " suites cached, "
                                                  << reuse_exe_suites.size() << " exe reusable, "
                                                  << uncached_suites.size()
                                                  << " need recompilation");
        }
    } else {
        uncached_suites = std::move(suites);
    }

    // 5c. Longest-job-first scheduling: sort uncached suites by estimated duration
    // (descending) using timing data from the previous cache run. This minimises
    // thread idle time at the tail of the compilation phase — inspired by LLVM LIT.
    if (use_cache && !uncached_suites.empty()) {
        std::stable_sort(uncached_suites.begin(), uncached_suites.end(),
                         [&](const Suite& a, const Suite& b) {
                             const auto* ea = cache.get(a.name);
                             const auto* eb = cache.get(b.name);
                             // Unknown suites go first (INT64_MAX) to avoid scheduling them last
                             int64_t ta = ea ? (ea->duration_us + ea->compile_time_us) : INT64_MAX;
                             int64_t tb = eb ? (eb->duration_us + eb->compile_time_us) : INT64_MAX;
                             return ta > tb; // descending: longest first
                         });
    }

    // 6. Parallel compilation (independent pipeline via QueryContext)
    std::atomic<bool> should_stop{false};
    std::vector<SuiteRunResult> exec_results;
    std::vector<CompileResult> compile_results; // kept in scope for cache update (compile_time_us)
    std::set<std::string> all_covered_functions;

    if (!uncached_suites.empty()) {
        CompileConfig compile_config;
        compile_config.verbose = config.verbose;
        compile_config.coverage = config.coverage;
        compile_config.no_cache = config.no_cache;
        compile_config.num_threads = config.compile_threads;

        compile_results = compile_suites_parallel(uncached_suites, compile_config, should_stop);

        // Count compilation errors
        for (const auto& cr : compile_results) {
            if (!cr.success)
                result.compilation_errors++;
        }

        // 7. Parallel execution (with coverage collection if enabled)
        exec_results =
            execute_suites_parallel(uncached_suites, compile_results, config, should_stop,
                                    config.coverage ? &all_covered_functions : nullptr);
    }

    // 7b. Execute suites with reusable exe (source unchanged, exe cached — skip recompile)
    if (!reuse_exe_suites.empty()) {
        // Build CompileResults pointing at existing cached exes (no actual compilation)
        std::vector<CompileResult> reuse_compile_results;
        reuse_compile_results.reserve(reuse_exe_suites.size());
        for (const auto& exe : reuse_exe_paths) {
            CompileResult cr;
            cr.success = true;
            cr.exe_path = exe;
            reuse_compile_results.push_back(std::move(cr));
        }
        auto reuse_exec =
            execute_suites_parallel(reuse_exe_suites, reuse_compile_results, config, should_stop,
                                    config.coverage ? &all_covered_functions : nullptr);
        for (auto& r : reuse_exec)
            exec_results.push_back(std::move(r));
        // Append to uncached_suites and compile_results for unified cache update in step 8
        for (auto& s : reuse_exe_suites)
            uncached_suites.push_back(std::move(s));
        for (auto& cr : reuse_compile_results)
            compile_results.push_back(std::move(cr));
    }

    // 8. Update cache with new results
    if (use_cache) {
        for (size_t i = 0; i < uncached_suites.size(); ++i) {
            auto& suite = uncached_suites[i];
            auto& sr = exec_results[i];

            std::vector<std::string> file_paths;
            file_paths.reserve(suite.tests.size());
            for (const auto& t : suite.tests)
                file_paths.push_back(t.file_path);
            auto source_hashes = TestResultCache::compute_source_hashes(file_paths);

            SuiteCacheEntry entry;
            entry.source_hashes = std::move(source_hashes);
            entry.flags_hash = flags_hash;
            entry.all_passed =
                sr.compile_ok && sr.failed == 0 && sr.crashed == 0 && sr.passed == sr.test_count;
            entry.test_count = sr.test_count;
            entry.passed_count = sr.passed;
            entry.failed_count = sr.failed;
            entry.duration_us = sr.exec_time_us;
            // Store compile time for longest-job-first scheduling on subsequent runs
            if (i < compile_results.size()) {
                entry.compile_time_us = compile_results[i].compile_time_us;
                // Always store exe_path (even for failing suites) so get_reusable_exe()
                // can skip recompilation on the next run if source is unchanged.
                entry.exe_path = compile_results[i].exe_path;
            }

            cache.update(suite.name, entry);
        }

        auto build_dir = fs::current_path() / "build" / "debug";
        std::error_code ec;
        fs::create_directories(build_dir, ec);
        auto cache_file = build_dir / ".new-test-cache.json";
        if (cache.save(cache_file.string())) {
            TML_LOG_INFO("test", "  [cache] Saved " << cache.size() << " suite entries to "
                                                    << cache_file.string());
        } else {
            TML_LOG_INFO("test", "  [cache] Failed to save cache to " << cache_file.string());
        }
    }

    // 9. Aggregate results (cached + executed)
    result.suites.reserve(cached_results.size() + exec_results.size());
    for (auto& sr : cached_results)
        result.suites.push_back(std::move(sr));
    for (auto& sr : exec_results)
        result.suites.push_back(std::move(sr));

    for (const auto& sr : result.suites) {
        result.total_tests += sr.test_count;
        result.passed += sr.passed;
        result.failed += sr.failed;
        result.crashed += sr.crashed;
        if (!sr.compile_ok)
            result.failed += sr.test_count;
    }

    auto total_end = Clock::now();
    result.total_duration_us =
        std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start).count();

    // 10. Coverage report generation (independent)
    if (config.coverage && !all_covered_functions.empty()) {
        CoverageStats cov_stats;
        cov_stats.total_tests = result.total_tests;
        cov_stats.total_files = static_cast<int>(test_files.size());
        cov_stats.failed_count = result.failed;
        cov_stats.compilation_error_count = result.compilation_errors;
        cov_stats.total_duration_ms = result.total_duration_us / 1000;
        cov_stats.no_cache = config.no_cache;

        // Populate per-suite info for HTML report — aggregate by group
        // (matches old system: multiple files in the same group are combined)
        std::map<std::string, SuiteInfo> suite_map;
        for (const auto& sr : result.suites) {
            std::string group_name = sr.group.empty() ? sr.name : sr.group;
            auto& si = suite_map[group_name];
            si.name = group_name;
            si.test_count += sr.test_count; // Total tests, not just passed
            si.duration_ms += sr.exec_time_us / 1000;
        }
        for (auto& [name, si] : suite_map) {
            cov_stats.suites.push_back(std::move(si));
        }
        // Sort by test count descending (largest suites first)
        std::sort(
            cov_stats.suites.begin(), cov_stats.suites.end(),
            [](const SuiteInfo& a, const SuiteInfo& b) { return a.test_count > b.test_count; });

        // Always print coverage to console (even partial runs)
        print_coverage_report(all_covered_functions, true, cov_stats);

        result.covered_functions_count = static_cast<int>(all_covered_functions.size());

        // GUARD: Only write coverage HTML/JSON/JSONL for FULL suite runs.
        // Partial runs (filtered by --suite or patterns) must NOT overwrite
        // the coverage report — this is a hard rule from the old system.
        bool is_partial_run = !config.suite_filters.empty() || !config.patterns.empty();

        if (is_partial_run) {
            TML_LOG_INFO(
                "test",
                "\033[33m[Coverage] Partial run detected (suite filter or pattern active)\033[0m");
            TML_LOG_INFO("test", "\033[33m[Coverage] HTML/JSON files will NOT be saved — run full "
                                 "suite for persistent coverage\033[0m");
        } else if (!CompilerOptions::coverage_output.empty()) {
            int current_covered = static_cast<int>(all_covered_functions.size());

            if (current_covered == 0) {
                // Zero coverage = instrumentation bug, never save
                TML_LOG_FATAL(
                    "test",
                    "\033[1;31m========================================================\033[0m");
                TML_LOG_FATAL("test",
                              "\033[1;31m  COVERAGE ABORTED: Zero functions tracked\033[0m");
                TML_LOG_FATAL("test",
                              "\033[1;31m  Tests ran but no coverage data was collected.\033[0m");
                TML_LOG_FATAL(
                    "test", "\033[1;31m  This indicates a bug in coverage instrumentation.\033[0m");
                TML_LOG_FATAL("test", "\033[1;31m  HTML/JSON files will NOT be generated.\033[0m");
                TML_LOG_FATAL(
                    "test",
                    "\033[1;31m========================================================\033[0m");
            } else {
                // Check for regression against previous report
                bool should_write = true;
                auto prev = get_previous_coverage_from_json(CompilerOptions::coverage_output);

                if (prev.valid && prev.total > 0) {
                    double current_pct = (100.0 * current_covered) / prev.total;
                    if (current_pct < prev.percent) {
                        TML_LOG_FATAL("test", "\033[1;31m=========================================="
                                              "==============\033[0m");
                        TML_LOG_FATAL("test", "\033[1;31m  COVERAGE REGRESSION DETECTED\033[0m");
                        TML_LOG_FATAL("test", "\033[1;31m  Previous: "
                                                  << prev.covered << "/" << prev.total << " ("
                                                  << std::fixed << std::setprecision(1)
                                                  << prev.percent << "%)\033[0m");
                        TML_LOG_FATAL("test", "\033[1;31m  Current:  "
                                                  << current_covered << "/" << prev.total << " ("
                                                  << std::fixed << std::setprecision(1)
                                                  << current_pct << "%)\033[0m");
                        TML_LOG_FATAL("test",
                                      "\033[1;31m  HTML/JSON files will NOT be updated.\033[0m");
                        TML_LOG_FATAL("test", "\033[1;31m=========================================="
                                              "==============\033[0m");
                        should_write = false;
                    }
                }

                if (should_write) {
                    write_coverage_html(all_covered_functions, CompilerOptions::coverage_output,
                                        cov_stats);
                    TML_LOG_INFO("test", "\033[1;32m[Coverage report updated successfully]\033[0m");
                }
            }
        }
    }

    return result;
}

} // namespace tml::testing
