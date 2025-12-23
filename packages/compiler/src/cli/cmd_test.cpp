#include "cmd_test.hpp"
#include "cmd_debug.hpp"
#include "cmd_build.hpp"
#include "compiler_setup.hpp"
#include "utils.hpp"
#include "tml/common.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <future>
#include <chrono>
#include <map>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
// Undefine Windows macros that conflict with std::min/max
#undef min
#undef max
#endif

namespace fs = std::filesystem;

namespace tml::cli {

// Enable ANSI colors on Windows
void enable_ansi_colors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

// Color helper that respects no_color option
struct ColorOutput {
    bool enabled;

    ColorOutput(bool use_color) : enabled(use_color) {}

    const char* reset() const { return enabled ? colors::reset : ""; }
    const char* bold() const { return enabled ? colors::bold : ""; }
    const char* dim() const { return enabled ? colors::dim : ""; }
    const char* red() const { return enabled ? colors::red : ""; }
    const char* green() const { return enabled ? colors::green : ""; }
    const char* yellow() const { return enabled ? colors::yellow : ""; }
    const char* blue() const { return enabled ? colors::blue : ""; }
    const char* cyan() const { return enabled ? colors::cyan : ""; }
    const char* gray() const { return enabled ? colors::gray : ""; }
    const char* magenta() const { return enabled ? colors::magenta : ""; }
};

std::string format_duration(int64_t ms) {
    std::ostringstream oss;
    if (ms < 1000) {
        oss << ms << "ms";
    } else if (ms < 60000) {
        oss << std::fixed << std::setprecision(2) << (ms / 1000.0) << "s";
    } else {
        int64_t minutes = ms / 60000;
        int64_t seconds = (ms % 60000) / 1000;
        oss << minutes << "m " << seconds << "s";
    }
    return oss.str();
}

std::string extract_group_name(const std::string& file_path) {
    fs::path path(file_path);

    // Look for common test directories in the path
    std::vector<std::string> parts;
    for (auto it = path.begin(); it != path.end(); ++it) {
        parts.push_back(it->string());
    }

    // Find "tests" or "tml" in the path and take directories after it
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i] == "tests" || parts[i] == "tml") {
            // Take the next directory as the group name
            if (i + 1 < parts.size() - 1) {  // -1 to exclude the filename
                std::string group = parts[i + 1];
                // If the next part is also a directory (not the file), include it
                if (i + 2 < parts.size() - 1) {
                    group += "/" + parts[i + 2];
                }
                return group;
            }
        }
    }

    // Fallback: use parent directory name
    return path.parent_path().filename().string();
}

TestOptions parse_test_args(int argc, char* argv[], int start_index) {
    TestOptions opts;

    for (int i = start_index; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--nocapture") {
            opts.nocapture = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
        } else if (arg == "--ignored") {
            opts.ignored = true;
        } else if (arg == "--bench") {
            opts.bench = true;
        } else if (arg == "--release") {
            opts.release = true;
        } else if (arg == "--no-color") {
            opts.no_color = true;
        } else if (arg.starts_with("--test-threads=")) {
            opts.test_threads = std::stoi(arg.substr(15));
        } else if (arg.starts_with("--timeout=")) {
            opts.timeout_seconds = std::stoi(arg.substr(10));
        } else if (arg.starts_with("--group=")) {
            opts.patterns.push_back(arg.substr(8));
        } else if (arg.starts_with("--suite=")) {
            opts.patterns.push_back(arg.substr(8));
        } else if (!arg.starts_with("--")) {
            opts.patterns.push_back(arg);
        }
    }

    return opts;
}

std::vector<std::string> discover_test_files(const std::string& root_dir) {
    std::vector<std::string> test_files;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                auto filename = path.filename().string();
                std::string path_str = path.string();

                // Skip files in errors/ or pending/ directories
                if (path_str.find("\\errors\\") != std::string::npos ||
                    path_str.find("/errors/") != std::string::npos ||
                    path_str.find("\\pending\\") != std::string::npos ||
                    path_str.find("/pending/") != std::string::npos) {
                    continue;
                }

                // Include .test.tml files or .tml files in tests/ directory
                if (filename.ends_with(".test.tml") ||
                    (path.extension() == ".tml" &&
                     (path_str.find("\\tests\\") != std::string::npos ||
                      path_str.find("/tests/") != std::string::npos))) {
                    test_files.push_back(path_str);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error discovering test files: " << e.what() << "\n";
    }

    // Remove duplicates and sort
    std::sort(test_files.begin(), test_files.end());
    test_files.erase(std::unique(test_files.begin(), test_files.end()), test_files.end());

    return test_files;
}

TestResult compile_and_run_test_with_result(const std::string& test_file, const TestOptions& opts) {
    TestResult result;
    result.file_path = test_file;
    result.test_name = fs::path(test_file).stem().string();
    result.group = extract_group_name(test_file);

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::string> empty_args;
    std::string captured_output;

    // Run test with timeout using async
    auto future = std::async(std::launch::async, [&]() {
        // Use quiet mode (output captured) unless nocapture
        // --verbose only controls test runner output format, not compiler debug
        if (opts.nocapture) {
            // nocapture mode shows all output live
            return run_run(test_file, empty_args, false);
        } else {
            // Default: capture output (shown on failure)
            return run_run_quiet(test_file, empty_args, false, &captured_output);
        }
    });

    // Wait for test to complete or timeout
    auto timeout_duration = std::chrono::seconds(opts.timeout_seconds);
    auto status = future.wait_for(timeout_duration);

    auto end_time = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    if (status == std::future_status::timeout) {
        result.passed = false;
        result.timeout = true;
        result.error_message = "Timeout exceeded " + std::to_string(opts.timeout_seconds) + "s";
        return result;
    }

    result.exit_code = future.get();
    result.passed = (result.exit_code == 0);

    if (!result.passed) {
        result.error_message = "Exit code: " + std::to_string(result.exit_code);
        // Store captured output for failed tests
        if (!captured_output.empty()) {
            result.error_message += "\n" + captured_output;
        }
    }

    return result;
}

int compile_and_run_test(const std::string& test_file, const TestOptions& opts) {
    auto result = compile_and_run_test_with_result(test_file, opts);

    if (!opts.quiet) {
        if (result.timeout) {
            std::cout << "test " << fs::path(test_file).filename().string()
                      << " ... TIMEOUT (exceeded " << opts.timeout_seconds << "s)\n";
        } else if (!result.passed) {
            std::cout << "test " << fs::path(test_file).filename().string()
                      << " ... FAILED (exit code: " << result.exit_code << ")\n";
        } else {
            std::cout << "test " << fs::path(test_file).filename().string() << " ... ok\n";
        }
    }

    return result.passed ? 0 : 1;
}

// Thread-safe result collector
struct TestResultCollector {
    std::mutex mutex;
    std::vector<TestResult> results;

    void add(TestResult result) {
        std::lock_guard<std::mutex> lock(mutex);
        results.push_back(std::move(result));
    }
};

// Thread worker for parallel test execution
void test_worker_new(
    const std::vector<std::string>& test_files,
    std::atomic<size_t>& current_index,
    TestResultCollector& collector,
    const TestOptions& opts
) {
    while (true) {
        size_t index = current_index.fetch_add(1);
        if (index >= test_files.size()) {
            break;
        }

        const auto& file = test_files[index];
        TestResult result = compile_and_run_test_with_result(file, opts);
        collector.add(std::move(result));
    }
}

// Print test results in Vitest style
void print_results_vitest_style(
    const std::vector<TestResult>& results,
    const TestOptions& opts,
    int64_t total_duration_ms
) {
    ColorOutput c(!opts.no_color);

    // Group results by directory
    std::map<std::string, TestGroup> groups;

    for (const auto& result : results) {
        auto& group = groups[result.group];
        if (group.name.empty()) {
            group.name = result.group;
        }
        group.results.push_back(result);
        group.total_duration_ms += result.duration_ms;
        if (result.passed) {
            group.passed++;
        } else {
            group.failed++;
        }
    }

    // Sort groups by name
    std::vector<std::string> group_names;
    for (const auto& [name, _] : groups) {
        group_names.push_back(name);
    }
    std::sort(group_names.begin(), group_names.end());

    // Print header
    std::cout << "\n";

    // Print each group
    for (const auto& group_name : group_names) {
        const auto& group = groups[group_name];

        // Group header with icon
        bool all_passed = (group.failed == 0);
        const char* icon = all_passed ? "+" : "x";
        const char* icon_color = all_passed ? c.green() : c.red();

        std::cout << " " << icon_color << icon << c.reset() << " "
                  << c.bold() << group.name << c.reset()
                  << " " << c.gray() << "(" << group.results.size() << " test"
                  << (group.results.size() != 1 ? "s" : "") << ")" << c.reset()
                  << " " << c.dim() << format_duration(group.total_duration_ms) << c.reset()
                  << "\n";

        // Print individual tests in group (only if verbose or there are failures)
        if (opts.verbose || group.failed > 0) {
            for (const auto& result : group.results) {
                const char* test_icon = result.passed ? "+" : "x";
                const char* test_color = result.passed ? c.green() : c.red();

                std::cout << "   " << test_color << test_icon << c.reset()
                          << " " << result.test_name;

                if (!result.passed) {
                    std::cout << " " << c.red() << "[" << result.error_message << "]" << c.reset();
                }

                if (opts.verbose) {
                    std::cout << " " << c.dim() << format_duration(result.duration_ms) << c.reset();
                }

                std::cout << "\n";
            }
        }
    }

    // Count totals
    int total_passed = 0;
    int total_failed = 0;
    for (const auto& result : results) {
        if (result.passed) {
            total_passed++;
        } else {
            total_failed++;
        }
    }

    // Print summary box
    std::cout << "\n";
    std::cout << " " << c.bold() << "Test Files  " << c.reset();
    if (total_failed > 0) {
        std::cout << c.red() << c.bold() << total_failed << " failed" << c.reset() << " | ";
    }
    std::cout << c.green() << c.bold() << total_passed << " passed" << c.reset()
              << " " << c.gray() << "(" << results.size() << ")" << c.reset() << "\n";

    std::cout << " " << c.bold() << "Duration    " << c.reset()
              << format_duration(total_duration_ms) << "\n";

    // Print final status line
    std::cout << "\n";
    if (total_failed == 0) {
        std::cout << " " << c.green() << c.bold() << "All tests passed!" << c.reset() << "\n";
    } else {
        std::cout << " " << c.red() << c.bold() << "Some tests failed." << c.reset() << "\n";
    }
    std::cout << "\n";
}

int run_test(int argc, char* argv[], bool verbose) {
    // Enable ANSI colors on Windows
    enable_ansi_colors();

    TestOptions opts = parse_test_args(argc, argv, 2);
    opts.verbose = opts.verbose || verbose;

    // Important: Don't propagate verbose to compiler debug output
    // Test --verbose only controls test runner output format
    // Compiler debug output is controlled separately via global flag
    // (set by dispatcher before calling run_test)
    tml::CompilerOptions::verbose = false;

    ColorOutput c(!opts.no_color);

    std::string cwd = fs::current_path().string();
    std::vector<std::string> test_files = discover_test_files(cwd);

    if (test_files.empty()) {
        if (!opts.quiet) {
            std::cout << c.yellow() << "No test files found" << c.reset()
                      << " (looking for *.test.tml)\n";
        }
        return 0;
    }

    // Filter test files by pattern
    if (!opts.patterns.empty()) {
        std::vector<std::string> filtered;
        for (const auto& file : test_files) {
            for (const auto& pattern : opts.patterns) {
                if (file.find(pattern) != std::string::npos) {
                    filtered.push_back(file);
                    break;
                }
            }
        }
        test_files = filtered;
    }

    if (test_files.empty()) {
        if (!opts.quiet) {
            std::cout << c.yellow() << "No tests matched the specified pattern(s)" << c.reset() << "\n";
        }
        return 0;
    }

    // Print header
    if (!opts.quiet) {
        std::cout << "\n " << c.cyan() << c.bold() << "TML" << c.reset()
                  << " " << c.dim() << "v0.1.0" << c.reset() << "\n";
        std::cout << "\n " << c.dim() << "Running " << test_files.size()
                  << " test file" << (test_files.size() != 1 ? "s" : "")
                  << "..." << c.reset() << "\n";
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    TestResultCollector collector;

    // Determine number of threads
    unsigned int num_threads = opts.test_threads;
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // Fallback
    }

    // Single-threaded mode for verbose/nocapture or if only 1 test
    if (opts.verbose || opts.nocapture || test_files.size() == 1 || num_threads == 1) {
        for (const auto& file : test_files) {
            TestResult result = compile_and_run_test_with_result(file, opts);
            collector.add(std::move(result));
        }
    } else {
        // Parallel execution
        std::atomic<size_t> current_index{0};
        std::vector<std::thread> threads;

        // Limit threads to number of tests
        num_threads = std::min(num_threads, static_cast<unsigned int>(test_files.size()));

        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back(
                test_worker_new,
                std::ref(test_files),
                std::ref(current_index),
                std::ref(collector),
                std::ref(opts)
            );
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    int64_t total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // Print results
    if (!opts.quiet) {
        print_results_vitest_style(collector.results, opts, total_duration_ms);
    }

    // Count failures
    int failed = 0;
    for (const auto& result : collector.results) {
        if (!result.passed) {
            failed++;
        }
    }

    return failed > 0 ? 1 : 0;
}

} // namespace tml::cli
