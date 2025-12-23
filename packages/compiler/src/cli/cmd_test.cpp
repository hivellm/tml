#include "cmd_test.hpp"
#include "cmd_debug.hpp"
#include "cmd_build.hpp"
#include "compiler_setup.hpp"
#include "utils.hpp"
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

namespace fs = std::filesystem;

namespace tml::cli {

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

int compile_and_run_test(const std::string& test_file, const TestOptions& opts) {
    if (opts.verbose) {
        std::cout << "Compiling and running test: " << test_file << "\n";
    }

    std::vector<std::string> empty_args;

    // Run test with timeout using async
    auto future = std::async(std::launch::async, [&]() {
        return run_run(test_file, empty_args, opts.verbose);
    });

    // Wait for test to complete or timeout
    auto timeout_duration = std::chrono::seconds(opts.timeout_seconds);
    auto status = future.wait_for(timeout_duration);

    int result = 0;
    if (status == std::future_status::timeout) {
        // Test timed out
        if (!opts.quiet) {
            std::cout << "test " << fs::path(test_file).filename().string()
                      << " ... TIMEOUT (exceeded " << opts.timeout_seconds << "s)\n";
        }
        return 1;
    } else {
        // Test completed
        result = future.get();
    }

    if (result != 0) {
        if (!opts.quiet) {
            std::cout << "test " << fs::path(test_file).filename().string() << " ... FAILED (exit code: " << result << ")\n";
        }
        return 1;
    }

    if (!opts.quiet) {
        std::cout << "test " << fs::path(test_file).filename().string() << " ... ok\n";
    }

    return 0;
}

// Thread worker for parallel test execution
void test_worker(
    const std::vector<std::string>& test_files,
    std::atomic<size_t>& current_index,
    std::atomic<int>& passed,
    std::atomic<int>& failed,
    std::mutex& output_mutex,
    const TestOptions& opts
) {
    (void)output_mutex;  // Reserved for future use
    while (true) {
        size_t index = current_index.fetch_add(1);
        if (index >= test_files.size()) {
            break;
        }

        const auto& file = test_files[index];
        int result = compile_and_run_test(file, opts);

        // Update counters atomically
        if (result == 0) {
            passed.fetch_add(1);
        } else {
            failed.fetch_add(1);
        }
    }
}

int run_test(int argc, char* argv[], bool verbose) {
    TestOptions opts = parse_test_args(argc, argv, 2);
    opts.verbose = opts.verbose || verbose;

    std::string cwd = fs::current_path().string();
    std::vector<std::string> test_files = discover_test_files(cwd);

    if (test_files.empty()) {
        if (!opts.quiet) {
            std::cout << "No test files found (looking for *.test.tml)\n";
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
            std::cout << "No tests matched the specified pattern(s)\n";
        }
        return 0;
    }

    if (!opts.quiet) {
        std::cout << "running " << test_files.size() << " test file(s)\n\n";
    }

    std::atomic<int> passed{0};
    std::atomic<int> failed{0};

    // Determine number of threads
    unsigned int num_threads = opts.test_threads;
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // Fallback
    }

    // Single-threaded mode for verbose/nocapture or if only 1 test
    if (opts.verbose || opts.nocapture || test_files.size() == 1 || num_threads == 1) {
        for (const auto& file : test_files) {
            int result = compile_and_run_test(file, opts);
            if (result == 0) {
                passed++;
            } else {
                failed++;
            }
        }
    } else {
        // Parallel execution
        std::atomic<size_t> current_index{0};
        std::mutex output_mutex;
        std::vector<std::thread> threads;

        // Limit threads to number of tests
        num_threads = std::min(num_threads, static_cast<unsigned int>(test_files.size()));

        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back(
                test_worker,
                std::ref(test_files),
                std::ref(current_index),
                std::ref(passed),
                std::ref(failed),
                std::ref(output_mutex),
                std::ref(opts)
            );
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

    // Print summary
    if (!opts.quiet) {
        std::cout << "\n";
        if (failed == 0) {
            std::cout << "test result: ok. ";
        } else {
            std::cout << "test result: FAILED. ";
        }
        std::cout << passed << " passed; " << failed << " failed\n";
    }

    return failed > 0 ? 1 : 0;
}

} // namespace tml::cli
