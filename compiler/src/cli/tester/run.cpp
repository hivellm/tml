//! # Test Runner Implementation
//!
//! This file contains the main `run_test()` function that orchestrates
//! test discovery, execution, and reporting.
//!
//! ## Test Flow
//!
//! ```text
//! run_test()
//!   ├─ parse_test_args()     → TestOptions
//!   ├─ discover_test_files() → List of *.test.tml files
//!   ├─ Filter by pattern(s)
//!   ├─ run_tests_suite_mode()  (parallel DLL compilation)
//!   └─ print_results_vitest_style()
//! ```
//!
//! All tests use the unified suite mode pipeline. Use `--no-suite` to
//! disable suite bundling (one DLL per test file).

#include "cli/builder/builder_internal.hpp"
#include "coverage.hpp"
#include "log/log.hpp"
#include "tester_internal.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Global unhandled exception filter for crash logging
static LONG WINAPI global_crash_filter(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;

    const char* name = "UNKNOWN";
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        name = "ACCESS_VIOLATION";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        name = "STACK_OVERFLOW";
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        name = "INTEGER_DIVIDE_BY_ZERO";
        break;
    }

    // Write directly to stderr handle for reliability
    char msg[256];
    int len = snprintf(msg, sizeof(msg),
                       "\n[FATAL CRASH] Exception 0x%08lX (%s)\n"
                       "Test crashed before exception could be caught.\n",
                       (unsigned long)code, name);

    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD written;
    WriteFile(hErr, msg, (DWORD)len, &written, NULL);
    FlushFileBuffers(hErr);

    return EXCEPTION_EXECUTE_HANDLER;
}

static void install_global_crash_handler() {
    SetUnhandledExceptionFilter(global_crash_filter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
}
#endif

namespace tml::cli {

// Using tester namespace for internal functions
using namespace tester;

// ============================================================================
// Parse Test Arguments
// ============================================================================

/// Parses test command-line arguments into a TestOptions struct.
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
            opts.coverage = true; // Implicitly enable coverage
        } else if (arg == "--coverage-source") {
            opts.coverage_source = true;
        } else if (arg.starts_with("--coverage-source-dir=")) {
            opts.coverage_source_dir = arg.substr(22);
            opts.coverage_source = true; // Implicitly enable source coverage
        } else if (arg == "--check-leaks") {
            opts.check_leaks = true;
        } else if (arg == "--no-check-leaks") {
            opts.check_leaks = false;
        } else if (arg == "--profile") {
            opts.profile = true;
        } else if (arg == "--suite") {
            opts.suite_mode = true; // Use suite-based DLL compilation (default)
        } else if (arg == "--no-suite") {
            opts.suite_mode = false; // Disable suite mode (one DLL per test file)
        } else if (arg == "--fail-fast" || arg == "-x") {
            opts.fail_fast = true; // Stop on first test failure
        } else if (arg == "--backtrace") {
            opts.backtrace = true; // Enable backtrace on test failures (default)
        } else if (arg == "--no-backtrace") {
            opts.backtrace = false; // Disable backtrace on test failures
        } else if (arg.starts_with("--test-threads=")) {
            opts.test_threads = std::stoi(arg.substr(15));
        } else if (arg.starts_with("--timeout=")) {
            opts.timeout_seconds = std::stoi(arg.substr(10));
        } else if (arg.starts_with("--group=")) {
            opts.patterns.push_back(arg.substr(8));
        } else if (arg.starts_with("--suite=")) {
            opts.patterns.push_back(arg.substr(8));
        } else if (arg.starts_with("--backend=")) {
            opts.backend = arg.substr(10);
            if (opts.backend != "llvm" && opts.backend != "cranelift") {
                TML_LOG_ERROR("test",
                              "Unknown backend '" << opts.backend << "'. Valid: llvm, cranelift");
            }
        } else if (arg == "--polonius") {
            CompilerOptions::polonius = true;
        } else if (arg.starts_with("--feature=")) {
            opts.features.push_back(arg.substr(10));
        } else if (arg == "--feature" && i + 1 < argc) {
            opts.features.push_back(argv[++i]);
        } else if (!arg.starts_with("--")) {
            opts.patterns.push_back(arg);
        }
    }

    return opts;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int run_test(int argc, char* argv[], bool verbose) {
#ifdef _WIN32
    // Install global crash handler to log crashes that escape SEH
    install_global_crash_handler();
#endif

    // Enable ANSI colors on Windows
    enable_ansi_colors();

    TestOptions opts = parse_test_args(argc, argv, 2);
    opts.verbose = opts.verbose || verbose;

    // Important: Don't propagate verbose to compiler debug output
    // Test --verbose only controls test runner output format
    tml::CompilerOptions::verbose = false;

    // Set coverage options (global flag for runtime linking + output path)
    tml::CompilerOptions::coverage = opts.coverage;
    if (!opts.coverage_output.empty()) {
        tml::CompilerOptions::coverage_output = opts.coverage_output;
    } else if (opts.coverage) {
        // Default coverage output to build/coverage directory to keep project root clean
        fs::path coverage_dir = fs::path("build") / "coverage";
        fs::create_directories(coverage_dir);
        tml::CompilerOptions::coverage_output = (coverage_dir / "coverage.html").string();
    }

    // Set LLVM source coverage options
    tml::CompilerOptions::coverage_source = opts.coverage_source;
    if (!opts.coverage_source_dir.empty()) {
        tml::CompilerOptions::coverage_source_dir = opts.coverage_source_dir;
    }

    // Set memory leak checking option
    tml::CompilerOptions::check_leaks = opts.check_leaks;

    // When --verbose is active, add a filtered JSON file sink to the logger
    // so test log output goes to build/debug/test_log.json.
    // Only "test" and "build" modules at INFO+ are written to avoid bloating
    // the file with compiler DEBUG spam (234k+ entries, 26MB+).
    if (opts.verbose) {
        fs::path log_dir = fs::path("build") / "debug";
        fs::create_directories(log_dir);
        fs::path log_path = log_dir / "test_log.json";

        // Filtered sink: wraps a FileSink and only writes test/build module messages
        class TestLogSink : public tml::log::LogSink {
        public:
            TestLogSink(const std::string& path) : inner_(path, /*append=*/false) {
                inner_.set_format(tml::log::LogFormat::JSON);
            }
            void write(const tml::log::LogRecord& record) override {
                // Only write test-related modules, skip compiler/codegen noise
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

    ColorOutput c(!opts.no_color);

    // If --bench flag is set, run benchmarks instead of tests
    if (opts.bench) {
        return run_benchmarks(opts, c);
    }

    // If --fuzz flag is set, run fuzz tests instead
    if (opts.fuzz) {
        return run_fuzz_tests(opts, c);
    }

    std::string cwd = fs::current_path().string();
    std::vector<std::string> test_files = discover_test_files(cwd);

    if (test_files.empty()) {
        if (!opts.quiet) {
            TML_LOG_INFO("test", c.yellow() << "No test files found" << c.reset()
                                            << " (looking for *.test.tml)");
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
            TML_LOG_INFO("test", c.yellow()
                                     << "No tests matched the specified pattern(s)" << c.reset());
        }
        return 0;
    }

    // Coverage cannot be used with filters - it requires full test suite
    if (opts.coverage && !opts.patterns.empty()) {
        TML_LOG_ERROR("test", "Coverage cannot be used with test filters");
        return 1;
    }

    // Print header (Rust-style)
    if (!opts.quiet) {
        TML_LOG_INFO("test", "running " << test_files.size() << " test file"
                                        << (test_files.size() != 1 ? "s" : ""));
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Initialize LLVM source coverage collector if enabled
    std::unique_ptr<tester::CoverageCollector> coverage_collector;
    if (opts.coverage_source) {
        coverage_collector = std::make_unique<tester::CoverageCollector>();
        if (!coverage_collector->initialize()) {
            TML_LOG_ERROR(
                "test", "Coverage initialization error: " << coverage_collector->get_last_error());
            return 1;
        }

        // Set up profraw output directory
        fs::path coverage_dir = fs::path(tml::CompilerOptions::coverage_source_dir);
        fs::path profraw_dir = coverage_dir / "profraw";
        coverage_collector->set_profraw_dir(profraw_dir);

        // Set environment variable for LLVM profile output
        // Use %p for process ID to handle parallel tests
        std::string profile_path = coverage_collector->get_profile_env("test");
#ifdef _WIN32
        _putenv_s("LLVM_PROFILE_FILE", profile_path.c_str());
#else
        setenv("LLVM_PROFILE_FILE", profile_path.c_str(), 1);
#endif

        if (!opts.quiet) {
            TML_LOG_INFO("test", c.dim() << "Source coverage enabled (output: "
                                         << coverage_dir.string() << ")" << c.reset());
        }
    }

    TestResultCollector collector;

    // Helper to process coverage after tests complete
    auto process_coverage = [&]() {
        if (!coverage_collector)
            return;

        fs::path coverage_dir = fs::path(tml::CompilerOptions::coverage_source_dir);
        fs::path profdata = coverage_dir / "coverage.profdata";

        coverage_collector->collect_profraw_files();

        if (!opts.quiet) {
            TML_LOG_INFO("test", c.dim() << "Generating coverage report..." << c.reset());
        }

        if (coverage_collector->merge_profiles(profdata)) {
            // Generate function-level coverage report from profdata
            // (Line-level coverage requires coverage mapping data which we don't generate yet)
            auto report = coverage_collector->generate_function_report(profdata);

            if (!opts.quiet && report.success) {
                coverage_collector->print_function_report(report);
            } else if (!opts.quiet && !report.success) {
                TML_LOG_WARN("test", report.error_message);
            }
        } else if (!opts.quiet) {
            TML_LOG_ERROR("test", "Coverage Error: " << coverage_collector->get_last_error());
        }
    };

    // When --no-cache is used, clean the .run-cache directory to remove accumulated
    // DLLs, object files, and other artifacts from previous test runs
    if (opts.no_cache) {
        fs::path run_cache_dir = build::get_run_cache_dir();
        if (fs::exists(run_cache_dir)) {
            TML_LOG_INFO("test", "Cleaning .run-cache directory...");
            std::error_code ec;
            size_t removed = 0;
            for (const auto& entry : fs::directory_iterator(run_cache_dir, ec)) {
                if (entry.is_regular_file()) {
                    fs::remove(entry.path(), ec);
                    if (!ec)
                        ++removed;
                }
            }
            if (removed > 0) {
                TML_LOG_INFO("test", "Removed " << removed << " cached files from .run-cache");
            }
        }
    }

    // Unified execution: all tests go through suite mode
    run_tests_suite_mode(test_files, opts, collector, c);

    auto end_time = std::chrono::high_resolution_clock::now();
    int64_t total_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Print results
    if (!opts.quiet) {
        print_results_vitest_style(collector.results, opts, total_duration_ms);

        // Print profiling stats if enabled
        if (opts.profile && collector.profile_stats.total_tests > 0) {
            print_profile_stats(collector.profile_stats, opts);
        }

        // Print TML runtime coverage summary
        if (opts.coverage && !CompilerOptions::coverage_output.empty()) {
            TML_LOG_INFO("test", c.dim() << "Coverage report: " << c.reset()
                                         << CompilerOptions::coverage_output);
        }
    }

    // Process LLVM source coverage
    process_coverage();

    // Flush log file and notify user
    if (opts.verbose) {
        tml::log::Logger::instance().flush();
        fs::path log_path = fs::path("build") / "debug" / "test_log.json";
        TML_LOG_INFO("test", c.dim() << "Test log: " << c.reset() << log_path.string());
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
