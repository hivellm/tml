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
#include "profiler.hpp"
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
// Crash-resilient cache save via atexit
// ============================================================================

static TestResultCache* g_atexit_cache = nullptr;
static std::string g_atexit_cache_path;

static void atexit_save_cache() {
    if (g_atexit_cache && !g_atexit_cache_path.empty()) {
        g_atexit_cache->save(g_atexit_cache_path);
    }
}

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
        // Cap at 4 concurrent test processes to avoid CPU saturation.
        // Each test process compiles + runs, consuming significant CPU.
        // Users can override with --test-threads=N.
        int hw = static_cast<int>(std::thread::hardware_concurrency());
        max_concurrent = std::max(1, std::min(4, hw / 2));
    }

    // Build list of work items.
    // run_all_mode=true:  one item per suite  (test_index=-1), subprocess gets --run-all
    // run_all_mode=false: one item per test   (test_index=N),  subprocess gets --test-index=N
    struct ExecWork {
        int suite_index;
        int test_index; // -1 means run-all (entire suite in one subprocess)
        std::string exe_path;
    };
    std::vector<ExecWork> pending;
    for (int i = 0; i < static_cast<int>(compile_results.size()); ++i) {
        if (compile_results[i].success) {
            const auto& suite = suites[i];
            if (config.run_all_mode) {
                // One work item per suite — subprocess runs all tests sequentially
                pending.push_back({i, -1, compile_results[i].exe_path});
            } else {
                // Legacy: one work item per test for crash isolation
                for (int t = 0; t < static_cast<int>(suite.tests.size()); ++t) {
                    pending.push_back({i, t, compile_results[i].exe_path});
                }
            }
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

    // Initialize result structures for successful suites
    for (int i = 0; i < static_cast<int>(compile_results.size()); ++i) {
        if (compile_results[i].success) {
            auto& r = results[i];
            auto& suite = suites[i];
            r.name = suite.name;
            r.group = suite.group;
            r.test_count = static_cast<int>(suite.tests.size());
            r.compile_ok = true;
            r.compile_time_us = compile_results[i].compile_time_us;
            r.tests.resize(suite.tests.size());
            for (int t = 0; t < static_cast<int>(suite.tests.size()); ++t) {
                r.tests[t].index = t;
                r.tests[t].name = suite.tests[t].test_name;
                r.tests[t].file = suite.tests[t].file_path;
            }
            // Propagate per-file compile errors
            for (const auto& pfe : compile_results[i].per_file_errors) {
                r.per_file_compile_errors.push_back({pfe.file_path, pfe.error});
            }
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
        int test_index; // -1 = run-all mode
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
            const auto& suite = suites[work.suite_index];

            ProcessOptions opts;
            opts.exe_path = work.exe_path;
            if (work.test_index < 0) {
                opts.args = {"--run-all"};
            } else {
                opts.args = {"--test-index=" + std::to_string(work.test_index)};
            }
            // Set subprocess timeout based on mode and per-test limit
            if (config.per_test_timeout_us > 0 && work.test_index < 0) {
                // run-all: timeout = per_test_limit * test_count + 2s headroom
                int test_count = static_cast<int>(suites[work.suite_index].tests.size());
                int64_t total_s = (config.per_test_timeout_us * test_count) / 1000000 + 2;
                opts.timeout = std::chrono::seconds(static_cast<int>(total_s));
            } else if (config.per_test_timeout_us > 0 && work.test_index >= 0) {
                // per-test: timeout = per_test_limit + 1s headroom
                int64_t total_s = config.per_test_timeout_us / 1000000 + 1;
                opts.timeout = std::chrono::seconds(static_cast<int>(total_s));
            } else {
                opts.timeout = std::chrono::seconds(config.timeout_seconds);
            }

            // Coverage: tell subprocess to write covered functions to a file
            if (config.coverage && out_covered) {
                auto cov_dir = fs::current_path() / "build" / "coverage";
                std::error_code ec;
                fs::create_directories(cov_dir, ec);
                if (work.test_index < 0) {
                    // run-all mode: one coverage file per suite
                    opts.env["TML_COVERAGE_FILE"] =
                        (cov_dir / ("cov_" + suite.name + ".txt")).string();
                } else {
                    // legacy mode: one coverage file per test
                    opts.env["TML_COVERAGE_FILE"] =
                        (cov_dir /
                         ("cov_" + suite.name + "_t" + std::to_string(work.test_index) + ".txt"))
                            .string();
                }

                // Redirect LLVM profraw files to build/coverage/profraw/
                auto profraw_dir = cov_dir / "profraw";
                fs::create_directories(profraw_dir, ec);
                std::string profraw_suffix =
                    work.test_index < 0 ? "" : "_t" + std::to_string(work.test_index);
                opts.env["LLVM_PROFILE_FILE"] =
                    (profraw_dir / (suite.name + profraw_suffix + "-%p%m.profraw")).string();
            }

            auto proc = Process::launch(opts);
            if (proc) {
                running.push_back(
                    {work.suite_index, work.test_index, std::move(*proc), "", "", now_us()});
            } else {
                auto& r = results[work.suite_index];
                if (work.test_index < 0) {
                    // run-all mode: mark all tests as failed to launch
                    for (auto& t : r.tests) {
                        t.passed = false;
                        t.error = "Failed to launch subprocess";
                    }
                    r.failed += static_cast<int>(r.tests.size());
                } else {
                    r.tests[work.test_index].passed = false;
                    r.tests[work.test_index].error = "Failed to launch subprocess";
                    r.failed++;
                }
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
                if (!proc_result.stderr_output.empty())
                    it->accumulated_stderr += proc_result.stderr_output;

                int si = it->suite_index;
                int ti = it->test_index;
                auto& sr = results[si];

                if (ti < 0) {
                    // ----------------------------------------------------------------
                    // run-all mode: parse a multi-test NDJSON stream.
                    // Track which tests have been started and which have been resolved.
                    // ----------------------------------------------------------------
                    std::set<int> started_indices;
                    std::set<int> resolved_indices;

                    std::istringstream stream(it->accumulated_stdout);
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
                                if constexpr (std::is_same_v<T, TestStartEvent>) {
                                    started_indices.insert(ev.index);
                                } else if constexpr (std::is_same_v<T, TestPassEvent>) {
                                    if (ev.index >= 0 &&
                                        ev.index < static_cast<int>(sr.tests.size())) {
                                        auto& t = sr.tests[ev.index];
                                        t.passed = true;
                                        t.duration_us = ev.duration_us;
                                        sr.passed++;
                                        resolved_indices.insert(ev.index);
                                    }
                                } else if constexpr (std::is_same_v<T, TestFailEvent>) {
                                    if (ev.index >= 0 &&
                                        ev.index < static_cast<int>(sr.tests.size())) {
                                        auto& t = sr.tests[ev.index];
                                        t.passed = false;
                                        t.exit_code = ev.exit_code;
                                        t.error = ev.error;
                                        t.duration_us = ev.duration_us;
                                        sr.failed++;
                                        resolved_indices.insert(ev.index);
                                    }
                                } else if constexpr (std::is_same_v<T, TestCrashEvent>) {
                                    if (ev.index >= 0 &&
                                        ev.index < static_cast<int>(sr.tests.size())) {
                                        auto& t = sr.tests[ev.index];
                                        t.passed = false;
                                        t.error = "CRASH: " + ev.signal;
                                        t.duration_us = ev.duration_us;
                                        sr.crashed++;
                                        resolved_indices.insert(ev.index);
                                    }
                                } else if constexpr (std::is_same_v<T, TestTimeoutEvent>) {
                                    if (ev.index >= 0 &&
                                        ev.index < static_cast<int>(sr.tests.size())) {
                                        auto& t = sr.tests[ev.index];
                                        t.passed = false;
                                        t.error = "TIMEOUT";
                                        sr.failed++;
                                        resolved_indices.insert(ev.index);
                                    }
                                }
                                // SuiteStartEvent and SuiteEndEvent are informational only
                            },
                            parsed.event);
                    }

                    // If the subprocess crashed mid-stream, handle unresolved tests.
                    // Tests that had test_start but no outcome = crashed.
                    // Tests that never got test_start = not reached (subprocess died).
                    if (proc_result.timed_out) {
                        // Mark all unresolved tests as timed out
                        for (int idx = 0; idx < static_cast<int>(sr.tests.size()); ++idx) {
                            if (resolved_indices.count(idx) == 0) {
                                sr.tests[idx].passed = false;
                                sr.tests[idx].error = "TIMEOUT";
                                sr.failed++;
                            }
                        }
                    } else if (proc_result.exit_code != 0 &&
                               resolved_indices.size() < sr.tests.size()) {
                        std::string crash_err = !it->accumulated_stderr.empty()
                                                    ? it->accumulated_stderr
                                                    : "Process crashed with exit code " +
                                                          std::to_string(proc_result.exit_code);
                        for (int idx = 0; idx < static_cast<int>(sr.tests.size()); ++idx) {
                            if (resolved_indices.count(idx) != 0)
                                continue;
                            auto& t = sr.tests[idx];
                            t.passed = false;
                            if (started_indices.count(idx)) {
                                // Started but no outcome = process crashed during this test
                                t.error = "CRASH: " + crash_err;
                                sr.crashed++;
                            } else {
                                // Never reached = subprocess died before this test
                                t.error = "NOT RUN: " + crash_err;
                                sr.crashed++;
                            }
                        }
                    } else if (resolved_indices.empty()) {
                        // exit 0 but no events at all — treat all tests as passed
                        for (auto& t : sr.tests) {
                            t.passed = true;
                            sr.passed++;
                        }
                    }

                    // Reclassify slow tests as skipped
                    if (config.per_test_timeout_us > 0) {
                        for (auto& t : sr.tests) {
                            if (t.passed && t.duration_us > config.per_test_timeout_us) {
                                // Mark as slow but still count as passed
                                // (test ran successfully, just took too long)
                                t.error = "SLOW: " + std::to_string(t.duration_us / 1000) +
                                          "ms (limit " +
                                          std::to_string(config.per_test_timeout_us / 1000) + "ms)";
                            }
                        }
                    }
                } else {
                    // ----------------------------------------------------------------
                    // Legacy per-test mode: single test per subprocess
                    // ----------------------------------------------------------------
                    auto& test_result = sr.tests[ti];

                    bool got_event = false;
                    std::istringstream stream(it->accumulated_stdout);
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
                                    test_result.passed = true;
                                    test_result.duration_us = ev.duration_us;
                                    sr.passed++;
                                    got_event = true;
                                } else if constexpr (std::is_same_v<T, TestFailEvent>) {
                                    test_result.passed = false;
                                    test_result.exit_code = ev.exit_code;
                                    test_result.error = ev.error;
                                    test_result.duration_us = ev.duration_us;
                                    sr.failed++;
                                    got_event = true;
                                } else if constexpr (std::is_same_v<T, TestCrashEvent>) {
                                    test_result.passed = false;
                                    test_result.error = "CRASH: " + ev.signal;
                                    test_result.duration_us = ev.duration_us;
                                    sr.crashed++;
                                    got_event = true;
                                } else if constexpr (std::is_same_v<T, TestTimeoutEvent>) {
                                    test_result.passed = false;
                                    test_result.error = "TIMEOUT";
                                    got_event = true;
                                }
                            },
                            parsed.event);
                    }

                    // Process crashed or timed out without emitting NDJSON events
                    if (!got_event) {
                        if (proc_result.timed_out) {
                            test_result.passed = false;
                            test_result.error = "TIMEOUT";
                            sr.failed++;
                        } else if (proc_result.exit_code != 0) {
                            test_result.passed = false;
                            test_result.exit_code = proc_result.exit_code;
                            test_result.error = !it->accumulated_stderr.empty()
                                                    ? it->accumulated_stderr
                                                    : "Process crashed with exit code " +
                                                          std::to_string(proc_result.exit_code);
                            sr.crashed++;
                        } else {
                            // exit 0 but no events — treat as pass (shouldn't happen normally)
                            test_result.passed = true;
                            sr.passed++;
                        }
                    }
                }

                // Accumulate exec time for the suite
                sr.exec_time_us += exec_us;

                // fail_fast only triggers on assertion failures, not crashes.
                // Crashes are isolated per-test and shouldn't stop the whole run.
                if (config.fail_fast && sr.failed > 0) {
                    should_stop.store(true, std::memory_order_relaxed);
                }

                // Read coverage data from output file
                if (config.coverage && out_covered) {
                    fs::path cov_file;
                    if (ti < 0) {
                        cov_file = fs::current_path() / "build" / "coverage" /
                                   ("cov_" + suites[si].name + ".txt");
                    } else {
                        cov_file = fs::current_path() / "build" / "coverage" /
                                   ("cov_" + suites[si].name + "_t" + std::to_string(ti) + ".txt");
                    }
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
// Debug Layers — emit multi-layer IR diagnostics for failing tests
// ============================================================================

/// Find the tml executable (same logic as MCP tools).
static std::string find_tml_exe() {
#ifdef _WIN32
    std::vector<std::string> paths = {
        "build/debug/bin/tml.exe",
        "build/debug/tml.exe",
        "build/release/bin/tml.exe",
        "tml.exe",
    };
#else
    std::vector<std::string> paths = {
        "build/debug/bin/tml",
        "build/debug/tml",
        "build/release/bin/tml",
        "tml",
    };
#endif
    for (const auto& p : paths) {
        if (fs::exists(p)) {
            return fs::absolute(p).string();
        }
    }
    return "tml";
}

/// Run a command and capture output (simplified popen wrapper).
static std::string run_capture(const std::string& cmd) {
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return "[debug-layers] Failed to run: " + cmd;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        result += buf;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

/// Read a text file into a string (returns empty on failure).
static std::string read_file_contents(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// Extract only the LLVM IR for a specific function from a .ll file.
/// Searches for `define ... @<func_name>(` and captures until the closing `}`.
static std::string extract_function_ir(const std::string& full_ir, const std::string& func_name) {
    // Search for the function definition
    std::string marker = "@" + func_name + "(";
    auto pos = full_ir.find(marker);
    if (pos == std::string::npos) {
        // Try mangled name patterns: @tml_<func_name> or @test_<func_name>
        marker = "@test_" + func_name + "(";
        pos = full_ir.find(marker);
    }
    if (pos == std::string::npos) {
        // Return first 200 lines as fallback
        std::string truncated;
        int lines = 0;
        for (size_t i = 0; i < full_ir.size() && lines < 200; ++i) {
            truncated += full_ir[i];
            if (full_ir[i] == '\n') {
                lines++;
            }
        }
        if (lines >= 200) {
            truncated += "\n... (truncated, " + std::to_string(full_ir.size()) + " bytes total)\n";
        }
        return truncated;
    }

    // Find the `define` keyword before the marker
    auto line_start = full_ir.rfind('\n', pos);
    if (line_start == std::string::npos) {
        line_start = 0;
    } else {
        line_start++;
    }

    // Find the closing `}` (function-level, at column 0)
    auto end_pos = pos;
    int brace_depth = 0;
    bool in_func = false;
    for (size_t i = line_start; i < full_ir.size(); ++i) {
        if (full_ir[i] == '{') {
            brace_depth++;
            in_func = true;
        } else if (full_ir[i] == '}') {
            brace_depth--;
            if (in_func && brace_depth == 0) {
                end_pos = i + 1;
                break;
            }
        }
    }

    return full_ir.substr(line_start, end_pos - line_start);
}

/// Extract a function from MIR text output by name.
/// MIR functions start with "fn <name>(" and end with "}" at indent 0.
static std::string extract_mir_function(const std::string& full_mir, const std::string& func_name) {
    // Search for "fn <name>" or "fn test_<name>"
    std::string marker = "fn " + func_name + "(";
    auto pos = full_mir.find(marker);
    if (pos == std::string::npos) {
        marker = "fn test_" + func_name + "(";
        pos = full_mir.find(marker);
    }
    if (pos == std::string::npos) {
        // Try substring match
        for (size_t i = 0; i < full_mir.size(); ++i) {
            if (full_mir.substr(i, 3) == "fn " &&
                full_mir.find(func_name, i) < full_mir.find('\n', i)) {
                pos = i;
                break;
            }
        }
    }
    if (pos == std::string::npos) {
        // Return first 100 lines as fallback
        std::string truncated;
        int lines = 0;
        for (size_t i = 0; i < full_mir.size() && lines < 100; ++i) {
            truncated += full_mir[i];
            if (full_mir[i] == '\n') {
                lines++;
            }
        }
        if (lines >= 100) {
            truncated += "\n... (truncated)\n";
        }
        return truncated;
    }

    // Find the start of the line containing "fn"
    auto line_start = full_mir.rfind('\n', pos);
    line_start = (line_start == std::string::npos) ? 0 : line_start + 1;

    // Find closing "}" at brace depth 0
    auto end_pos = pos;
    int brace_depth = 0;
    bool in_func = false;
    for (size_t i = line_start; i < full_mir.size(); ++i) {
        if (full_mir[i] == '{') {
            brace_depth++;
            in_func = true;
        } else if (full_mir[i] == '}') {
            brace_depth--;
            if (in_func && brace_depth == 0) {
                end_pos = i + 1;
                break;
            }
        }
    }

    return full_mir.substr(line_start, end_pos - line_start);
}

/// Analyze error text and emitted IR layers to generate diagnosis hints.
/// Looks for common patterns that indicate which compilation layer has the bug.
static std::string generate_diagnosis_hints(const std::string& error_text,
                                            const std::string& test_name) {
    std::string hints;
    hints += "\n\n=== DIAGNOSIS HINTS ===\n";

    bool has_hir = error_text.find("=== HIR") != std::string::npos;
    bool has_mir = error_text.find("=== MIR") != std::string::npos;
    (void)has_hir; // Used in condition below

    // Pattern: "Could not generate" indicates compilation failure at that layer
    bool hir_failed = error_text.find("Could not generate HIR") != std::string::npos;
    bool mir_failed = error_text.find("Could not generate MIR") != std::string::npos;
    bool ir_failed = error_text.find("Could not generate IR") != std::string::npos;

    if (hir_failed) {
        hints += "Layer: PARSER or TYPE SYSTEM\n";
        hints += "Symptom: HIR generation failed — source didn't parse or type-check\n";
        hints += "Possible causes:\n";
        hints += "  - Syntax error in source file\n";
        hints += "  - Unresolved type or import\n";
        hints += "  - Missing impl for a behavior\n";
        return hints;
    }

    if (mir_failed && has_hir) {
        hints += "Layer: HIR → MIR LOWERING\n";
        hints += "Symptom: HIR succeeded but MIR generation failed\n";
        hints += "Possible causes:\n";
        hints += "  - Monomorphization failure (generic not instantiated)\n";
        hints += "  - Closure capture analysis bug\n";
        hints += "  - Desugaring produced invalid HIR\n";
        return hints;
    }

    if (ir_failed && has_mir) {
        hints += "Layer: CODEGEN (MIR → LLVM IR)\n";
        hints += "Symptom: MIR succeeded but LLVM IR generation failed\n";
        hints += "Possible causes:\n";
        hints += "  - Type layout mismatch in codegen\n";
        hints += "  - Missing instruction handler in MirCodegen\n";
        hints += "  - ABI/calling convention error\n";
        return hints;
    }

    // All layers generated — analyze content patterns
    // Pattern: assertion failure (runtime bug)
    if (error_text.find("assert") != std::string::npos ||
        error_text.find("ASSERT") != std::string::npos) {
        // Check for common codegen patterns in the LLVM IR
        bool has_sret = error_text.find("sret") != std::string::npos;
        bool has_void_call = error_text.find("call void") != std::string::npos;
        bool has_type_mismatch = error_text.find("type mismatch") != std::string::npos ||
                                 error_text.find("invalid type") != std::string::npos;

        if (has_type_mismatch) {
            hints += "Layer: CODEGEN\n";
            hints += "Symptom: Type mismatch in generated LLVM IR\n";
            hints += "Possible causes:\n";
            hints += "  - Struct passed by value when pointer expected (or vice versa)\n";
            hints += "  - sret convention mismatch between caller and callee\n";
            hints += "  - Integer width mismatch (i32 vs i64)\n";
        } else if (has_sret && has_void_call) {
            hints += "Layer: CODEGEN (calling convention)\n";
            hints += "Symptom: sret + void call pattern — possible return value corruption\n";
            hints += "Possible causes:\n";
            hints += "  - Function returns struct but caller expects void\n";
            hints += "  - sret parameter not properly forwarded\n";
        } else {
            hints += "Layer: RUNTIME or LIBRARY\n";
            hints += "Symptom: Assertion failure with all compilation layers looking correct\n";
            hints += "Possible causes:\n";
            hints += "  - Logic error in TML library code\n";
            hints += "  - C runtime function returning wrong value\n";
            hints += "  - Memory layout mismatch between TML and C runtime\n";
        }
    } else if (error_text.find("exit code") != std::string::npos ||
               error_text.find("crashed") != std::string::npos) {
        hints += "Layer: CODEGEN or RUNTIME\n";
        hints += "Symptom: Process crashed (non-assertion failure)\n";
        hints += "Possible causes:\n";
        hints += "  - Null pointer dereference from incorrect codegen\n";
        hints += "  - Stack corruption from ABI mismatch\n";
        hints += "  - Use-after-free from incorrect drop ordering\n";
    } else {
        hints += "Layer: UNKNOWN\n";
        hints += "Symptom: Test failed but pattern not recognized\n";
        hints += "Action: Compare HIR → MIR → LLVM IR manually for the failing function\n";
    }

    if (!test_name.empty()) {
        hints += "Focus function: " + test_name + "\n";
    }

    return hints;
}

/// For each failing test, emit MIR + LLVM IR and append to the test's error message.
static void emit_debug_layers_for_failures(TestRunResult& result) {
    std::string tml_exe = find_tml_exe();
    std::set<std::string> already_emitted; // Avoid duplicate for same source file

    for (auto& suite : result.suites) {
        for (auto& test : suite.tests) {
            if (test.passed || test.file.empty()) {
                continue;
            }

            // Normalize the source file path
            std::string src_file = test.file;
            std::replace(src_file.begin(), src_file.end(), '\\', '/');

            if (already_emitted.count(src_file) > 0) {
                test.error += "\n\n=== DEBUG LAYERS ===\n(see above — same source file)\n";
                continue;
            }
            already_emitted.insert(src_file);

            fs::path src_path(src_file);
            std::string stem = src_path.stem().string();

            // --- HIR Layer ---
            TML_LOG_INFO("test", "[debug-layers] Emitting HIR for: " << src_file);
            std::string hir_cmd =
                "\"" + tml_exe + "\" build \"" + src_file + "\" --emit-hir --legacy 2>&1";
            run_capture(hir_cmd);

            fs::path hir_path = fs::path("build/debug") / (stem + ".hir");
            if (fs::exists(hir_path)) {
                std::string full_hir = read_file_contents(hir_path);
                if (!full_hir.empty()) {
                    // HIR uses "func <name>" syntax, same extraction as MIR
                    std::string hir_section;
                    if (!test.name.empty()) {
                        hir_section = extract_mir_function(full_hir, test.name);
                    } else {
                        hir_section = extract_mir_function(full_hir, "");
                    }
                    test.error += "\n\n=== HIR (--debug-layers) ===\n" + hir_section;
                }
            } else {
                test.error += "\n\n=== HIR ===\n[debug-layers] Could not generate HIR\n";
            }

            // --- MIR Layer ---
            TML_LOG_INFO("test", "[debug-layers] Emitting MIR for: " << src_file);
            std::string mir_cmd =
                "\"" + tml_exe + "\" build \"" + src_file + "\" --emit-mir --legacy 2>&1";
            run_capture(mir_cmd);

            fs::path mir_path = fs::path("build/debug") / (stem + ".mir");
            if (fs::exists(mir_path)) {
                std::string full_mir = read_file_contents(mir_path);
                if (!full_mir.empty()) {
                    std::string mir_section;
                    if (!test.name.empty()) {
                        mir_section = extract_mir_function(full_mir, test.name);
                    } else {
                        mir_section = extract_mir_function(full_mir, "");
                    }
                    test.error += "\n\n=== MIR (--debug-layers) ===\n" + mir_section;
                }
            } else {
                test.error += "\n\n=== MIR ===\n[debug-layers] Could not generate MIR\n";
            }

            // --- LLVM IR Layer ---
            TML_LOG_INFO("test", "[debug-layers] Emitting LLVM IR for: " << src_file);
            std::string ir_cmd =
                "\"" + tml_exe + "\" build \"" + src_file + "\" --emit-ir --legacy 2>&1";
            run_capture(ir_cmd);

            fs::path ll_path = fs::path("build/debug") / (stem + ".ll");
            if (fs::exists(ll_path)) {
                std::string full_ir = read_file_contents(ll_path);
                if (!full_ir.empty()) {
                    std::string ir_section;
                    if (!test.name.empty()) {
                        ir_section = extract_function_ir(full_ir, test.name);
                    } else {
                        ir_section = extract_function_ir(full_ir, "");
                    }
                    test.error += "\n\n=== LLVM IR (--debug-layers) ===\n" + ir_section;
                }
            } else {
                test.error += "\n\n=== LLVM IR ===\n[debug-layers] Could not generate IR\n";
            }

            // --- Diagnosis Hints ---
            // Analyze the error message and emitted layers to suggest the likely bug layer
            test.error += generate_diagnosis_hints(test.error, test.name);
        }
    }
}

// ============================================================================
// Entry point
// ============================================================================

TestRunResult run_tests(const TestConfig& config) {
    TML_ZONE("test::run_tests");
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

    // 4. Suite grouping
    // For full suite runs (no filters) WITHOUT coverage, use larger suites to
    // reduce link count. 206 suites → ~30 suites saves ~170 link steps.
    // Coverage mode keeps max_per_suite=1: each test .obj has internal linkage
    // for stdlib functions, and vtables (which are external/linkonce_odr) reference
    // those internal functions — linking multiple .objs causes undefined symbols.
    int effective_max_per_suite = config.max_per_suite;
    bool is_full_run = config.suite_filters.empty() && config.patterns.empty();
    if (is_full_run && !config.coverage && effective_max_per_suite <= 10) {
        effective_max_per_suite = 50; // ~30 suites instead of ~206
        TML_LOG_INFO("test",
                     "[coordinator] Full suite: using max_per_suite=50 for fewer link steps");
    }
    auto suites = group_into_suites(test_files, effective_max_per_suite);
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

    // ========================================================================
    // PER-SUITE PATH: the standard compilation path.
    // When running full suite (no filters), uses larger suites (max_per_suite=50)
    // to reduce link count from ~206 to ~30. This is the Zig-inspired optimization:
    // fewer binaries = fewer link steps = much faster.
    // ========================================================================

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
            TML_LOG_INFO("test",
                         "[cache] Compiler changed — downgrading cached suites to exe-reusable");
            cache.downgrade_to_exe_reusable();
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
    // Cache source hashes to avoid recomputing in step 8
    std::map<std::string, std::vector<std::string>> suite_source_hashes;

    if (use_cache) {
        for (auto& suite : suites) {
            std::vector<std::string> file_paths;
            file_paths.reserve(suite.tests.size());
            for (const auto& t : suite.tests)
                file_paths.push_back(t.file_path);
            auto source_hashes = TestResultCache::compute_source_hashes(file_paths);
            // Store for reuse in step 8
            suite_source_hashes[suite.name] = source_hashes;

            if (cache.is_cached(suite.name, source_hashes, flags_hash)) {
                const auto* entry = cache.get(suite.name);
                // In coverage mode, cached suites must still RUN to produce coverage data.
                // Downgrade to exe-reusable: skip recompilation but re-execute.
                if (config.coverage && !entry->exe_path.empty() && fs::exists(entry->exe_path)) {
                    TML_LOG_INFO("test", "  [cache] Suite " << suite.name
                                                            << " cached but coverage needs re-run");
                    reuse_exe_paths.push_back(entry->exe_path);
                    reuse_exe_suites.push_back(std::move(suite));
                } else {
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
                    TML_LOG_INFO("test", "  [cache] Suite " << suite.name
                                                            << " is cached (passed, skipping)");
                }
            } else if (auto exe = cache.get_reusable_exe(suite.name, source_hashes, flags_hash);
                       !exe.empty()) {
                // Source unchanged + exe exists: skip recompilation, just re-run
                TML_LOG_INFO("test", "  [cache] Suite " << suite.name
                                                        << " exe reusable (source unchanged, "
                                                           "skipping recompilation)");
                reuse_exe_paths.push_back(std::move(exe));
                reuse_exe_suites.push_back(std::move(suite));
            } else {
                // Fallback: check if exe exists on disk even without cache entry
                // Only for non-coverage runs — coverage exes must be compiled with instrumentation
                if (!config.coverage) {
                    auto exe_cache_dir = fs::current_path() / "build" / "debug" / ".new-run-cache";
                    auto disk_exe = exe_cache_dir / (suite.name + ".exe");
                    if (fs::exists(disk_exe)) {
                        TML_LOG_INFO("test", "  [cache] Suite "
                                                 << suite.name
                                                 << " exe found on disk (skip recompile)");
                        reuse_exe_paths.push_back(disk_exe.string());
                        reuse_exe_suites.push_back(std::move(suite));
                    } else {
                        uncached_suites.push_back(std::move(suite));
                    }
                } else {
                    uncached_suites.push_back(std::move(suite));
                }
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

    // 5c. Longest-job-first scheduling (disabled: caused memory issues with large cache)
    // TODO: re-enable once the root cause of the sort-related crash is fixed

    // 6. Parallel compilation (independent pipeline via QueryContext)
    std::atomic<bool> should_stop{false};
    std::vector<SuiteRunResult> exec_results;
    std::vector<CompileResult> compile_results; // kept in scope for cache update (compile_time_us)
    std::set<std::string> all_covered_functions;

    // Cache file path for incremental saves
    auto cache_file_path =
        (fs::current_path() / "build" / "debug" / ".new-test-cache.json").string();
    {
        std::error_code ec;
        fs::create_directories(fs::current_path() / "build" / "debug", ec);
    }

    // Register atexit handler to save cache even if LLVM calls exit()
    if (use_cache) {
        g_atexit_cache = &cache;
        g_atexit_cache_path = cache_file_path;
        static bool registered = false;
        if (!registered) {
            std::atexit(atexit_save_cache);
            registered = true;
        }
    }

    // Helper: update cache entries and flush to disk immediately.
    // Called after compilation and execution to ensure crash-resilient caching.
    auto update_cache_entries = [&](const std::vector<Suite>& flush_suites,
                                    const std::vector<SuiteRunResult>* flush_results,
                                    const std::vector<CompileResult>& flush_compile_results) {
        for (int i = 0; i < static_cast<int>(flush_suites.size()); ++i) {
            auto& suite = flush_suites[i];

            std::vector<std::string> source_hashes;
            auto hash_it = suite_source_hashes.find(suite.name);
            if (hash_it != suite_source_hashes.end()) {
                source_hashes = hash_it->second;
            } else {
                std::vector<std::string> file_paths;
                file_paths.reserve(suite.tests.size());
                for (const auto& t : suite.tests)
                    file_paths.push_back(t.file_path);
                source_hashes = TestResultCache::compute_source_hashes(file_paths);
            }

            SuiteCacheEntry entry;
            entry.source_hashes = std::move(source_hashes);
            entry.flags_hash = flags_hash;

            if (flush_results && i < static_cast<int>(flush_results->size())) {
                auto& sr = (*flush_results)[i];
                entry.all_passed = sr.compile_ok && sr.failed == 0 && sr.crashed == 0 &&
                                   sr.passed == sr.test_count;
                entry.test_count = sr.test_count;
                entry.passed_count = sr.passed;
                entry.failed_count = sr.failed;
                entry.duration_us = sr.exec_time_us;
            }
            if (i < static_cast<int>(flush_compile_results.size())) {
                entry.compile_time_us = flush_compile_results[i].compile_time_us;
                entry.exe_path = flush_compile_results[i].exe_path;
            }

            cache.update(suite.name, entry);
        }
        cache.save(cache_file_path);
    };

    // 6. Execute reuse-exe suites FIRST (ensures results are saved even if compilation crashes)
    if (!reuse_exe_suites.empty()) {
        TML_LOG_INFO("test", "[coordinator] Executing " << reuse_exe_suites.size()
                                                        << " reuse-exe suites (skip recompile)");
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

        // Flush cache with execution results (marks passing suites as all_passed)
        update_cache_entries(reuse_exe_suites, &reuse_exec, reuse_compile_results);

        for (auto& r : reuse_exec) {
            exec_results.push_back(std::move(r));
        }
    }

    // 7. Compile + execute uncached suites
    if (!uncached_suites.empty()) {
        CompileConfig compile_config;
        compile_config.verbose = config.verbose;
        compile_config.coverage = config.coverage;
        compile_config.no_cache = config.no_cache;
        // Default to 1 compile thread for stability; user can override.
        compile_config.num_threads = config.compile_threads > 0 ? config.compile_threads : 1;

        // (incremental cache saves happen after each batch below)

        // Compile in batches to avoid memory exhaustion / LLVM state corruption
        // when compiling 100+ suites in a single process.
        int batch_size = 40;
        // Limit total suites compiled per run to avoid LLVM global state deadlocks
        int total = static_cast<int>(uncached_suites.size());
        if (config.max_compile_suites > 0 && total > config.max_compile_suites) {
            total = config.max_compile_suites;
            batch_size = std::min(batch_size, total);
            TML_LOG_INFO("test", "[coordinator] Limiting to "
                                     << total << " suites this run (--max-compile)");
        }
        std::vector<CompileResult> new_compile_results(uncached_suites.size());
        for (int batch_start = 0; batch_start < total; batch_start += batch_size) {
            int batch_end = std::min(batch_start + batch_size, total);
            int batch_count = batch_end - batch_start;

            // Create a sub-vector for this batch
            std::vector<Suite> batch_suites(
                std::make_move_iterator(uncached_suites.begin() + batch_start),
                std::make_move_iterator(uncached_suites.begin() + batch_end));

            TML_LOG_INFO("test", "[coordinator] Compiling batch "
                                     << (batch_start / batch_size + 1) << " (" << batch_count
                                     << " suites, " << batch_start << "-" << batch_end << " of "
                                     << total << ")");

            // Save cache after each suite compiles (crash resilience)
            std::mutex cache_save_mtx;
            CompileCallback batch_callback = nullptr;
            if (use_cache) {
                batch_callback = [&](int idx, const CompileResult& cr) {
                    const auto& suite = batch_suites[idx];
                    auto hash_it = suite_source_hashes.find(suite.name);
                    SuiteCacheEntry entry;
                    if (hash_it != suite_source_hashes.end()) {
                        entry.source_hashes = hash_it->second;
                    }
                    entry.flags_hash = flags_hash;
                    entry.compile_time_us = cr.compile_time_us;
                    entry.exe_path = cr.exe_path;
                    {
                        std::lock_guard<std::mutex> lock(cache_save_mtx);
                        cache.update(suite.name, entry);
                        cache.save(cache_file_path);
                    }
                };
            }

            auto batch_results =
                compile_suites_parallel(batch_suites, compile_config, should_stop, batch_callback);

            // Move results into the main vector
            for (int i = 0; i < batch_count; ++i) {
                new_compile_results[batch_start + i] = std::move(batch_results[i]);
            }

            // Move suites back (they were moved out for the batch)
            for (int i = 0; i < batch_count; ++i) {
                uncached_suites[batch_start + i] = std::move(batch_suites[i]);
            }

            // Post-batch cache save (safety: callback already saved per-suite)
            if (use_cache) {
                cache.save(cache_file_path);
                TML_LOG_INFO("test", "  [cache] Saved " << cache.size() << " entries after batch "
                                                        << (batch_start / batch_size + 1));
            }
        }

        // Count compilation errors
        for (const auto& cr : new_compile_results) {
            if (!cr.success) {
                result.compilation_errors++;
            }
        }

        // Final flush after compilation
        if (use_cache) {
            cache.save(cache_file_path);
            TML_LOG_INFO("test",
                         "  [cache] Flushed " << cache.size() << " entries after compilation");
        }

        // 7a. Parallel execution
        auto new_exec_results =
            execute_suites_parallel(uncached_suites, new_compile_results, config, should_stop,
                                    config.coverage ? &all_covered_functions : nullptr);

        // Flush cache after execution (saves pass/fail results)
        update_cache_entries(uncached_suites, &new_exec_results, new_compile_results);
        TML_LOG_INFO("test", "  [cache] Flushed " << cache.size() << " entries after execution");

        for (auto& r : new_exec_results) {
            exec_results.push_back(std::move(r));
        }
        for (auto& cr : new_compile_results) {
            compile_results.push_back(std::move(cr));
        }
    }

    // 8. Final cache save (all batches merged)
    TML_LOG_INFO("test",
                 "  [cache] Saved " << cache.size() << " suite entries to " << cache_file_path);

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

    // 9.5. Debug layers: emit LLVM IR for failing tests
    if (config.debug_layers && result.failed > 0) {
        emit_debug_layers_for_failures(result);
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
