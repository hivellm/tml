// Test command helpers - utilities and color output

#include "tester_internal.hpp"

namespace tml::cli::tester {

// ============================================================================
// Enable ANSI Colors on Windows
// ============================================================================

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

// ============================================================================
// Format Duration
// ============================================================================

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

// ============================================================================
// Extract Group Name
// ============================================================================

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
            if (i + 1 < parts.size() - 1) { // -1 to exclude the filename
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

// ============================================================================
// Test Result Collector
// ============================================================================

void TestResultCollector::add(TestResult result) {
    std::lock_guard<std::mutex> lock(mutex);

    // If this is a compilation error and we haven't seen one yet, store it
    if (result.compilation_error && !compilation_error_occurred.load()) {
        compilation_error_occurred.store(true);
        first_compilation_error = result;
    }

    results.push_back(std::move(result));
}

void TestResultCollector::add_timings(const PhaseTimings& timings) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& [phase, us] : timings.timings_us) {
        profile_stats.add(phase, us);
    }
    profile_stats.total_tests++;
}

bool TestResultCollector::has_compilation_error() const {
    return compilation_error_occurred.load();
}

} // namespace tml::cli::tester
