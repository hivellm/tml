TML_MODULE("compiler")

//! # Test Command Implementation
//!
//! Single entry point for `tml test`. Uses the coordinator-based test runner
//! with EXE subprocess execution. No legacy dependencies.

#include "cmd_test.hpp"

#include "common.hpp"
#include "log/log.hpp"
#include "testing/testing_coordinator.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tml::cli {

// ============================================================================
// ANSI Color Support
// ============================================================================

static void enable_ansi_colors() {
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

// Simple ANSI color wrapper
struct Colors {
    bool enabled;
    Colors(bool use_color) : enabled(use_color) {}
    const char* reset() const {
        return enabled ? "\033[0m" : "";
    }
    const char* bold() const {
        return enabled ? "\033[1m" : "";
    }
    const char* dim() const {
        return enabled ? "\033[2m" : "";
    }
    const char* red() const {
        return enabled ? "\033[31m" : "";
    }
    const char* green() const {
        return enabled ? "\033[32m" : "";
    }
    const char* yellow() const {
        return enabled ? "\033[33m" : "";
    }
    const char* cyan() const {
        return enabled ? "\033[36m" : "";
    }
};

// ============================================================================
// Argument Parsing
// ============================================================================

static TestOptions parse_args(int argc, char* argv[], int start_index) {
    TestOptions opts;

    for (int i = start_index; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--nocapture") {
            opts.nocapture = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
        } else if (arg == "--bench") {
            opts.bench = true;
        } else if (arg == "--fuzz") {
            opts.fuzz = true;
        } else if (arg.starts_with("--fuzz-duration=")) {
            opts.fuzz_duration = std::stoi(arg.substr(16));
            opts.fuzz = true;
        } else if (arg.starts_with("--fuzz-max-len=")) {
            opts.fuzz_max_len = std::stoi(arg.substr(15));
        } else if (arg.starts_with("--corpus=")) {
            opts.corpus_dir = arg.substr(9);
        } else if (arg.starts_with("--crashes=")) {
            opts.crashes_dir = arg.substr(10);
        } else if (arg == "--release") {
            opts.release = true;
        } else if (arg == "--no-color") {
            opts.no_color = true;
        } else if (arg == "--no-cache" || arg == "--no-cache!" || arg == "--force-no-cache") {
            opts.no_cache = true;
        } else if (arg.starts_with("--save-baseline=")) {
            opts.save_baseline = arg.substr(16);
        } else if (arg.starts_with("--compare=")) {
            opts.compare_baseline = arg.substr(10);
        } else if (arg == "--coverage") {
            opts.coverage = true;
        } else if (arg.starts_with("--coverage-output=")) {
            opts.coverage_output = arg.substr(18);
            opts.coverage = true;
        } else if (arg == "--coverage-source") {
            opts.coverage_source = true;
        } else if (arg.starts_with("--coverage-source-dir=")) {
            opts.coverage_source_dir = arg.substr(22);
            opts.coverage_source = true;
        } else if (arg == "--check-leaks") {
            opts.check_leaks = true;
        } else if (arg == "--no-check-leaks") {
            opts.check_leaks = false;
        } else if (arg == "--profile") {
            opts.profile = true;
        } else if (arg.starts_with("--log=")) {
            opts.log_path = arg.substr(6);
            opts.verbose = true;
        } else if (arg == "--fail-fast" || arg == "-x") {
            opts.fail_fast = true;
        } else if (arg == "--no-fail-fast") {
            opts.fail_fast = false;
        } else if (arg.starts_with("--test-threads=")) {
            opts.test_threads = std::stoi(arg.substr(15));
        } else if (arg.starts_with("--timeout=")) {
            opts.timeout_seconds = std::stoi(arg.substr(10));
        } else if (arg.starts_with("--group=") || arg.starts_with("--suite=")) {
            auto eq = arg.find('=');
            opts.suite_filters.push_back(arg.substr(eq + 1));
        } else if (arg.starts_with("--filter=")) {
            opts.patterns.push_back(arg.substr(9));
        } else if (arg == "--filter" && i + 1 < argc) {
            opts.patterns.push_back(argv[++i]);
        } else if (arg == "--list-suites") {
            opts.list_suites = true;
        } else if (arg.starts_with("--backend=")) {
            opts.backend = arg.substr(10);
        } else if (arg == "--polonius") {
            CompilerOptions::polonius = true;
        } else if (arg.starts_with("--feature=")) {
            opts.features.push_back(arg.substr(10));
        } else if (arg == "--feature" && i + 1 < argc) {
            opts.features.push_back(argv[++i]);
        } else if (arg == "--new-runner") {
            // Ignored - new runner is now the default and only runner
        } else if (!arg.starts_with("--")) {
            opts.patterns.push_back(arg);
        }
    }

    return opts;
}

// ============================================================================
// Test Command Entry Point
// ============================================================================

int run_test(int argc, char* argv[], bool verbose) {
    enable_ansi_colors();

    TestOptions opts = parse_args(argc, argv, 2);
    opts.verbose = opts.verbose || verbose;

    // Don't propagate verbose to compiler debug output
    tml::CompilerOptions::verbose = false;

    // Set coverage options
    tml::CompilerOptions::coverage = opts.coverage;
    if (!opts.coverage_output.empty()) {
        tml::CompilerOptions::coverage_output = opts.coverage_output;
    } else if (opts.coverage) {
        fs::path coverage_dir = fs::path("build") / "coverage";
        fs::create_directories(coverage_dir);
        tml::CompilerOptions::coverage_output = (coverage_dir / "coverage.html").string();
    }

    tml::CompilerOptions::coverage_source = opts.coverage_source;
    if (!opts.coverage_source_dir.empty()) {
        tml::CompilerOptions::coverage_source_dir = opts.coverage_source_dir;
    }

    tml::CompilerOptions::check_leaks = opts.check_leaks;

    // When --verbose is active, add a filtered JSON file sink to the logger
    if (opts.verbose) {
        fs::path log_path;
        if (!opts.log_path.empty()) {
            log_path = fs::path(opts.log_path);
            fs::create_directories(log_path.parent_path());
        } else {
            fs::path log_dir = fs::path("build") / "debug";
            fs::create_directories(log_dir);
            log_path = log_dir / "test_log.json";
        }

        class TestLogSink : public tml::log::LogSink {
        public:
            TestLogSink(const std::string& path) : inner_(path, /*append=*/false) {
                inner_.set_format(tml::log::LogFormat::JSON);
            }
            void write(const tml::log::LogRecord& record) override {
                if (record.module == "test" || record.module == "build" ||
                    record.level >= tml::log::LogLevel::Error) {
                    inner_.write(record);
                }
            }
            void flush() override {
                inner_.flush();
            }

        private:
            tml::log::FileSink inner_;
        };

        tml::log::Logger::instance().add_sink(std::make_unique<TestLogSink>(log_path.string()));
    }

    Colors c(!opts.no_color);

    // Benchmarks and fuzzing: not yet reimplemented
    if (opts.bench) {
        TML_LOG_ERROR("test", "Benchmarks not yet supported. Coming soon.");
        return 1;
    }
    if (opts.fuzz) {
        TML_LOG_ERROR("test", "Fuzz tests not yet supported. Coming soon.");
        return 1;
    }

    // ======================================================================
    // Coordinator-based test runner
    // ======================================================================
    testing::TestConfig tc;
    tc.patterns = opts.patterns;
    tc.suite_filters = opts.suite_filters;
    tc.max_per_suite = 10;
    tc.compile_threads = opts.test_threads;
    tc.exec_concurrent = 0; // auto
    tc.timeout_seconds = opts.timeout_seconds;
    tc.no_cache = opts.no_cache;
    tc.verbose = opts.verbose;
    tc.coverage = opts.coverage;
    tc.fail_fast = opts.coverage ? false : opts.fail_fast;
    tc.list_suites = opts.list_suites;

    auto result = testing::run_tests(tc);

    // Print summary
    if (!opts.quiet) {
        TML_LOG_INFO("test", "");
        TML_LOG_INFO("test", c.bold() << "Test Results" << c.reset());
        int cached_count = 0;
        for (const auto& s : result.suites) {
            if (s.cached)
                ++cached_count;
        }
        TML_LOG_INFO("test",
                     "  Suites:  " << result.suites.size()
                                   << (cached_count > 0
                                           ? " (" + std::to_string(cached_count) + " cached)"
                                           : ""));
        TML_LOG_INFO("test", "  Tests:   " << result.total_tests);
        TML_LOG_INFO("test", "  " << c.green() << "Passed:  " << result.passed << c.reset());
        if (result.failed > 0) {
            TML_LOG_INFO("test", "  " << c.red() << "Failed:  " << result.failed << c.reset());
        }
        if (result.crashed > 0) {
            TML_LOG_INFO("test", "  " << c.red() << "Crashed: " << result.crashed << c.reset());
        }
        if (result.compilation_errors > 0) {
            TML_LOG_INFO("test", "  " << c.yellow() << "Compile errors: "
                                      << result.compilation_errors << c.reset());
        }
        TML_LOG_INFO("test", "  Duration: " << (result.total_duration_us / 1000) << "ms");
        if (result.covered_functions_count > 0) {
            TML_LOG_INFO("test", "  " << c.cyan() << "Covered: " << result.covered_functions_count
                                      << " functions" << c.reset());
        }
        if (opts.coverage && !CompilerOptions::coverage_output.empty()) {
            TML_LOG_INFO("test", c.dim() << "  Coverage report: " << c.reset()
                                         << CompilerOptions::coverage_output);
        }

        // Print failed test details
        if (result.failed > 0 || result.crashed > 0 || result.compilation_errors > 0) {
            TML_LOG_INFO("test", "");
            TML_LOG_INFO("test", c.bold() << "Failure Details" << c.reset());
        }
        for (const auto& suite : result.suites) {
            if (!suite.compile_ok) {
                TML_LOG_ERROR("test", c.red() << "COMPILE ERROR" << c.reset() << " " << suite.name
                                              << ": " << suite.compile_error);
            }
            for (const auto& [file, err] : suite.per_file_compile_errors) {
                TML_LOG_ERROR("test", c.yellow() << "  COMPILE SKIP" << c.reset() << " " << file
                                                 << ": " << err);
            }
            for (const auto& test : suite.tests) {
                if (!test.passed) {
                    TML_LOG_ERROR("test",
                                  c.red() << "FAIL" << c.reset() << " " << suite.group << "/"
                                          << test.name << " (" << test.file << ")"
                                          << (test.exit_code != 0
                                                  ? " [exit " + std::to_string(test.exit_code) + "]"
                                                  : ""));
                    if (!test.error.empty()) {
                        std::istringstream err_stream(test.error);
                        std::string err_line;
                        int line_count = 0;
                        while (std::getline(err_stream, err_line) && line_count < 20) {
                            if (!err_line.empty() && err_line.back() == '\r')
                                err_line.pop_back();
                            if (!err_line.empty()) {
                                TML_LOG_ERROR("test", "    " << err_line);
                                ++line_count;
                            }
                        }
                        if (line_count >= 20) {
                            TML_LOG_ERROR("test",
                                          "    " << c.dim() << "... (truncated)" << c.reset());
                        }
                    }
                }
            }
        }
    }

    // Flush log
    if (opts.verbose) {
        tml::log::Logger::instance().flush();
        fs::path log_path = !opts.log_path.empty() ? fs::path(opts.log_path)
                                                   : fs::path("build") / "debug" / "test_log.json";
        TML_LOG_INFO("test", c.dim() << "Test log: " << c.reset() << log_path.string());
    }

    return (result.failed > 0 || result.crashed > 0 || result.compilation_errors > 0) ? 1 : 0;
}

} // namespace tml::cli
