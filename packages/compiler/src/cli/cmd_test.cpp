#include "cmd_test.hpp"
#include "cmd_debug.hpp"
#include "compiler_setup.hpp"
#include "utils.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

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
        } else if (!arg.starts_with("--")) {
            // Test name pattern
            opts.patterns.push_back(arg);
        }
    }

    return opts;
}

std::vector<std::string> discover_test_files(const std::string& root_dir) {
    std::vector<std::string> test_files;

    // Search for *.test.tml files recursively
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                auto filename = path.filename().string();

                // Match *.test.tml
                if (filename.ends_with(".test.tml")) {
                    test_files.push_back(path.string());
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error discovering test files: " << e.what() << "\n";
    }

    // Also check tests/ directory
    std::string tests_dir = root_dir + "/tests";
    if (fs::exists(tests_dir) && fs::is_directory(tests_dir)) {
        try {
            for (const auto& entry : fs::recursive_directory_iterator(tests_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".tml") {
                    test_files.push_back(entry.path().string());
                }
            }
        } catch (const fs::filesystem_error&) {
            // tests/ dir might not exist, that's ok
        }
    }

    std::sort(test_files.begin(), test_files.end());
    return test_files;
}

int compile_and_run_test(const std::string& test_file, const TestOptions& opts) {
    if (opts.verbose) {
        std::cout << "Compiling test: " << test_file << "\n";
    }

    // Build the test file
    // For now, just run it through check
    int result = run_check(test_file.c_str(), opts.verbose);

    if (result != 0) {
        std::cerr << "Test compilation failed: " << test_file << "\n";
        return result;
    }

    // TODO: When code generation is complete:
    // 1. Compile test file with --test flag
    // 2. Link with test runtime
    // 3. Execute test binary
    // 4. Parse and display results

    if (!opts.quiet) {
        std::cout << "test " << fs::path(test_file).filename().string() << " ... ok\n";
    }

    return 0;
}

int run_test(int argc, char* argv[], bool verbose) {
    TestOptions opts = parse_test_args(argc, argv, 2);
    opts.verbose = opts.verbose || verbose;

    // Get current working directory
    std::string cwd = fs::current_path().string();

    // Discover test files
    std::vector<std::string> test_files = discover_test_files(cwd);

    if (test_files.empty()) {
        if (!opts.quiet) {
            std::cout << "No test files found (looking for *.test.tml)\n";
        }
        return 0;
    }

    // Filter test files by pattern if provided
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

    // Print test run header
    if (!opts.quiet) {
        std::cout << "running " << test_files.size() << " test file(s)\n\n";
    }

    // Run each test file
    int passed = 0;
    int failed = 0;

    for (const auto& file : test_files) {
        int result = compile_and_run_test(file, opts);
        if (result == 0) {
            passed++;
        } else {
            failed++;
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
