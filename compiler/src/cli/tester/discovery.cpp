// Test command - file discovery

#include "tester_internal.hpp"

namespace tml::cli::tester {

// ============================================================================
// Discover Benchmark Files
// ============================================================================

std::vector<std::string> discover_bench_files(const std::string& root_dir) {
    std::vector<std::string> bench_files;

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

                // Include .bench.tml files
                if (filename.ends_with(".bench.tml")) {
                    bench_files.push_back(path_str);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error discovering benchmark files: " << e.what() << "\n";
    }

    // Sort by name
    std::sort(bench_files.begin(), bench_files.end());
    return bench_files;
}

// ============================================================================
// Discover Test Files
// ============================================================================

std::vector<std::string> discover_test_files(const std::string& root_dir) {
    // Try to use cached test list (valid for 1 hour)
    fs::path cache_file = fs::path(root_dir) / "build" / "debug" / ".test-cache";

    if (fs::exists(cache_file)) {
        auto cache_time = fs::last_write_time(cache_file);
        auto now = fs::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - cache_time);

        // Use cache if less than 1 hour old
        if (age.count() < 3600) {
            std::vector<std::string> test_files;
            std::ifstream cache_in(cache_file);
            std::string line;
            while (std::getline(cache_in, line)) {
                if (!line.empty() && fs::exists(line)) {
                    test_files.push_back(line);
                }
            }
            if (!test_files.empty()) {
                return test_files;
            }
        }
    }

    // Scan filesystem
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
                // But exclude .bench.tml files (those are for --bench)
                if (filename.ends_with(".bench.tml")) {
                    continue;
                }
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

    // Write to cache
    try {
        fs::create_directories(cache_file.parent_path());
        std::ofstream cache_out(cache_file);
        for (const auto& file : test_files) {
            cache_out << file << "\n";
        }
    } catch (...) {
        // Ignore cache write errors
    }

    return test_files;
}

} // namespace tml::cli::tester
