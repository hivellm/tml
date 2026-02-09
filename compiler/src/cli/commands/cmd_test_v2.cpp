//! # Test V2 Command Implementation (EXE-based)
//!
//! Entry point for `tml test-v2`. Uses the EXE-based subprocess execution
//! pipeline instead of DLL loading. Reuses argument parsing, test discovery,
//! and result formatting from the existing test framework.

#include "cmd_test_v2.hpp"

#include "cli/builder/builder_internal.hpp"
#include "cli/commands/cmd_test.hpp"
#include "cli/tester/exe_test_runner.hpp"
#include "cli/tester/tester_internal.hpp"
#include "common.hpp"
#include "log/log.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tml::cli {

using namespace tester;

int run_test_v2(int argc, char* argv[], bool verbose) {
#ifdef _WIN32
    // Enable ANSI colors on Windows
    enable_ansi_colors();
#endif

    TestOptions opts = parse_test_args(argc, argv, 2);
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
        fs::path log_dir = fs::path("build") / "debug";
        fs::create_directories(log_dir);
        fs::path log_path = log_dir / "test_log.json";

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

    ColorOutput c(!opts.no_color);

    // Benchmarks and fuzzing not supported in v2 (use tml test instead)
    if (opts.bench) {
        TML_LOG_ERROR("test", "Benchmarks not supported in test-v2. Use 'tml test --bench'.");
        return 1;
    }
    if (opts.fuzz) {
        TML_LOG_ERROR("test", "Fuzz tests not supported in test-v2. Use 'tml test --fuzz'.");
        return 1;
    }

    // Discover test files
    std::string cwd = fs::current_path().string();
    std::vector<std::string> test_files = discover_test_files(cwd);

    if (test_files.empty()) {
        if (!opts.quiet) {
            TML_LOG_INFO("test", c.yellow() << "No test files found" << c.reset()
                                            << " (looking for *.test.tml)");
        }
        return 0;
    }

    // Filter test files by pattern (normalize slashes for cross-platform matching)
    if (!opts.patterns.empty()) {
        auto normalize_slashes = [](std::string s) {
            std::replace(s.begin(), s.end(), '\\', '/');
            return s;
        };
        std::vector<std::string> filtered;
        for (const auto& file : test_files) {
            std::string norm_file = normalize_slashes(file);
            for (const auto& pattern : opts.patterns) {
                std::string norm_pattern = normalize_slashes(pattern);
                if (norm_file.find(norm_pattern) != std::string::npos) {
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

    // Coverage cannot be used with filters
    if (opts.coverage && !opts.patterns.empty()) {
        TML_LOG_ERROR("test", "Coverage cannot be used with test filters");
        return 1;
    }

    // Print header
    if (!opts.quiet) {
        TML_LOG_INFO("test", "[exe] running " << test_files.size() << " test file"
                                              << (test_files.size() != 1 ? "s" : "")
                                              << " (EXE mode)");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Clean run-cache if --no-cache
    if (opts.no_cache) {
        fs::path run_cache_dir = build::get_run_cache_dir();
        if (fs::exists(run_cache_dir)) {
            TML_LOG_INFO("test", "[exe] Cleaning .run-cache directory...");
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
                TML_LOG_INFO("test",
                             "[exe] Removed " << removed << " cached files from .run-cache");
            }
        }
    }

    TestResultCollector collector;

    // Run tests using EXE-based subprocess execution
    run_tests_exe_mode(test_files, opts, collector, c);

    auto end_time = std::chrono::high_resolution_clock::now();
    int64_t total_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Print results (reuse existing vitest-style formatting)
    if (!opts.quiet) {
        print_results_vitest_style(collector.results, opts, total_duration_ms);

        if (opts.profile && collector.profile_stats.total_tests > 0) {
            print_profile_stats(collector.profile_stats, opts);
        }

        if (opts.coverage && !CompilerOptions::coverage_output.empty()) {
            TML_LOG_INFO("test", c.dim() << "Coverage report: " << c.reset()
                                         << CompilerOptions::coverage_output);
        }
    }

    // Flush log
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
