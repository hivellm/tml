//! # LLVM Coverage Collector Implementation
//!
//! Implements coverage data collection and report generation using
//! LLVM's llvm-profdata and llvm-cov tools.

#include "coverage.hpp"

#include "cli/builder/compiler_setup.hpp"
#include "cli/commands/cmd_test.hpp"
#include "cli/utils.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace tml::cli::tester {

CoverageCollector::CoverageCollector() = default;
CoverageCollector::~CoverageCollector() = default;

bool CoverageCollector::initialize() {
    // Find llvm-profdata
    profdata_path_ = find_llvm_profdata();
    if (profdata_path_.empty()) {
        error_message_ = "llvm-profdata not found. Install LLVM or add it to PATH.";
        return false;
    }

    // Find llvm-cov
    cov_path_ = find_llvm_cov();
    if (cov_path_.empty()) {
        error_message_ = "llvm-cov not found. Install LLVM or add it to PATH.";
        return false;
    }

    return true;
}

void CoverageCollector::set_profraw_dir(const fs::path& dir) {
    profraw_dir_ = dir;
    fs::create_directories(profraw_dir_);
}

std::string CoverageCollector::get_profile_env(const std::string& test_name) {
    // Return path pattern for LLVM_PROFILE_FILE
    // %p = process ID (for parallel tests)
    // %m = merge pool - allows multiple processes/DLLs to contribute to same profile
    // This is important for DLL-based test execution
    fs::path profile_path = profraw_dir_ / (test_name + "-%p%m.profraw");
    return to_forward_slashes(profile_path.string());
}

void CoverageCollector::collect_profraw_files() {
    profraw_files_.clear();
    if (!fs::exists(profraw_dir_)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(profraw_dir_)) {
        if (entry.path().extension() == ".profraw") {
            profraw_files_.push_back(entry.path());
        }
    }
}

bool CoverageCollector::merge_profiles(const fs::path& output_profdata) {
    if (profraw_files_.empty()) {
        error_message_ = "No profraw files to merge";
        return false;
    }

    fs::path abs_output = fs::absolute(output_profdata);
    fs::create_directories(abs_output.parent_path());

    // Use a response file to avoid command line length limits on Windows
    // Write list of profraw files to a temp file
    fs::path response_file = profraw_dir_ / "profraw_files.txt";
    {
        std::ofstream rsp(response_file);
        if (!rsp.is_open()) {
            error_message_ = "Failed to create response file for profraw list";
            return false;
        }
        for (const auto& profraw : profraw_files_) {
            fs::path abs_profraw = fs::absolute(profraw);
            rsp << abs_profraw.string() << "\n";
        }
    }

    // Build command using input file list: llvm-profdata merge -sparse -input-files=file.txt -o out
    std::ostringstream cmd;
#ifdef _WIN32
    // On Windows, use full quoting for cmd.exe compatibility
    cmd << "\"\"" << profdata_path_ << "\" merge -sparse";
    cmd << " -input-files=\"" << response_file.string() << "\"";
    cmd << " -o \"" << abs_output.string() << "\"\"";
#else
    cmd << profdata_path_ << " merge -sparse";
    cmd << " -input-files=\"" << to_forward_slashes(response_file.string()) << "\"";
    cmd << " -o \"" << to_forward_slashes(abs_output.string()) << "\"";
#endif

    std::string command = cmd.str();
    std::system(command.c_str());

    // Cleanup response file
    fs::remove(response_file);

    if (!fs::exists(abs_output)) {
        error_message_ = "llvm-profdata merge failed - output file was not created";
        return false;
    }

    return true;
}

CoverageReport CoverageCollector::generate_report(const fs::path& binary, const fs::path& profdata,
                                                  const std::vector<fs::path>& source_dirs) {
    CoverageReport report;
    report.success = false;

    // Build command: llvm-cov report binary -instr-profile=profdata
    std::ostringstream cmd;
    cmd << "\"" << to_forward_slashes(cov_path_) << "\"";
    cmd << " report";
    cmd << " \"" << to_forward_slashes(binary.string()) << "\"";
    cmd << " -instr-profile=\"" << to_forward_slashes(profdata.string()) << "\"";

    // Add source filters if provided
    for (const auto& src_dir : source_dirs) {
        // Use -sources to filter which source files to include
        cmd << " -sources=\"" << to_forward_slashes(src_dir.string()) << "\"";
    }

    std::string command = cmd.str();

    // Execute and capture output
    // Create temp file for output
    fs::path temp_report = fs::temp_directory_path() / "tml_coverage_report.txt";
    command += " > \"" + to_forward_slashes(temp_report.string()) + "\" 2>&1";

    // Note: On Windows, system() may return non-zero even if the command succeeds
    std::system(command.c_str());

    // Parse the report output
    std::ifstream infile(temp_report);
    if (!infile.is_open()) {
        report.error_message = "Failed to read coverage report output";
        return report;
    }

    std::string line;
    bool in_file_section = false;

    // Parse llvm-cov report format:
    // Filename                      Regions    Missed Regions     Cover   Functions  Missed
    // Functions  Executed       Lines      Missed Lines     Cover    Branches   Missed Branches
    // Cover
    // ---                           ---                 ---        ---         --- ---       ---
    // ---               ---       ---         ---                ---       --- filename.cpp 100 10
    // 90.00%          20                2    90.00%         500                50    90.00% 200 20
    // 90.00%
    // ---                           ---                 ---        ---         --- ---       ---
    // ---               ---       ---         ---                ---       --- TOTAL 100
    // 10      90.00%          20                2    90.00%         500                50    90.00%
    // 200                20    90.00%

    while (std::getline(infile, line)) {
        // Skip empty lines and separators
        if (line.empty() || line.find("---") != std::string::npos) {
            continue;
        }

        // Skip header line
        if (line.find("Filename") != std::string::npos ||
            line.find("Regions") != std::string::npos) {
            in_file_section = true;
            continue;
        }

        if (!in_file_section) {
            continue;
        }

        // Parse file/total line
        std::istringstream iss(line);
        std::string filename;
        iss >> filename;

        if (filename.empty()) {
            continue;
        }

        // Read numeric values (simplified parsing)
        int regions, missed_regions;
        std::string cover1;
        int functions, missed_functions;
        std::string cover2;
        int lines, missed_lines;
        std::string cover3;
        int branches = 0, missed_branches = 0;

        iss >> regions >> missed_regions >> cover1;
        iss >> functions >> missed_functions >> cover2;
        iss >> lines >> missed_lines >> cover3;
        iss >> branches >> missed_branches;

        if (filename == "TOTAL") {
            report.summary.total_lines = lines;
            report.summary.covered_lines = lines - missed_lines;
            report.summary.total_functions = functions;
            report.summary.covered_functions = functions - missed_functions;
            report.summary.total_branches = branches;
            report.summary.covered_branches = branches - missed_branches;
        } else {
            FileCoverage fc;
            fc.file_path = filename;
            fc.total_lines = lines;
            fc.covered_lines = lines - missed_lines;
            fc.total_functions = functions;
            fc.covered_functions = functions - missed_functions;
            fc.total_branches = branches;
            fc.covered_branches = branches - missed_branches;
            report.files.push_back(fc);

            if (fc.covered_lines == 0 && fc.total_lines > 0) {
                report.uncovered_files.push_back(filename);
            }
        }
    }

    infile.close();
    fs::remove(temp_report);

    report.success = true;
    return report;
}

bool CoverageCollector::generate_html_report(const fs::path& binary, const fs::path& profdata,
                                             const fs::path& output_dir,
                                             const std::vector<fs::path>& source_dirs) {
    // Create output directory
    fs::create_directories(output_dir);

    // Build command: llvm-cov show binary -instr-profile=profdata -format=html -output-dir=dir
    std::ostringstream cmd;
    cmd << "\"" << to_forward_slashes(cov_path_) << "\"";
    cmd << " show";
    cmd << " \"" << to_forward_slashes(binary.string()) << "\"";
    cmd << " -instr-profile=\"" << to_forward_slashes(profdata.string()) << "\"";
    cmd << " -format=html";
    cmd << " -output-dir=\"" << to_forward_slashes(output_dir.string()) << "\"";

    // Add source filters if provided
    for (const auto& src_dir : source_dirs) {
        cmd << " -sources=\"" << to_forward_slashes(src_dir.string()) << "\"";
    }

    std::string command = cmd.str();

    // Note: On Windows, system() may return non-zero even if the command succeeds
    std::system(command.c_str());

    // Check if output was created (index.html should exist)
    fs::path index_html = output_dir / "index.html";
    return fs::exists(index_html);
}

bool CoverageCollector::generate_lcov_report(const fs::path& binary, const fs::path& profdata,
                                             const fs::path& output_lcov,
                                             const std::vector<fs::path>& source_dirs) {
    // Build command: llvm-cov export binary -instr-profile=profdata -format=lcov > output.lcov
    std::ostringstream cmd;
    cmd << "\"" << to_forward_slashes(cov_path_) << "\"";
    cmd << " export";
    cmd << " \"" << to_forward_slashes(binary.string()) << "\"";
    cmd << " -instr-profile=\"" << to_forward_slashes(profdata.string()) << "\"";
    cmd << " -format=lcov";

    // Add source filters if provided
    for (const auto& src_dir : source_dirs) {
        cmd << " -sources=\"" << to_forward_slashes(src_dir.string()) << "\"";
    }

    cmd << " > \"" << to_forward_slashes(output_lcov.string()) << "\"";

    std::string command = cmd.str();

    // Note: On Windows, system() may return non-zero even if the command succeeds
    std::system(command.c_str());

    return fs::exists(output_lcov);
}

void CoverageCollector::print_console_report(const CoverageReport& report) {
    using namespace colors;

    // Header
    TML_LOG_INFO("test", bold << " Coverage Report" << reset);
    TML_LOG_INFO("test", " " << std::string(60, '-'));
    {
        std::ostringstream hdr;
        hdr << std::left << std::setw(40) << " File" << std::right << std::setw(8) << "Lines"
            << std::setw(8) << "Branch" << std::setw(8) << "Funcs";
        TML_LOG_INFO("test", hdr.str());
    }
    TML_LOG_INFO("test", " " << std::string(60, '-'));

    // Helper to colorize percentage
    auto colorize_percent = [](double pct) -> std::string {
        if (pct >= 80.0)
            return std::string(colors::green);
        if (pct >= 50.0)
            return std::string(colors::yellow);
        return std::string(colors::red);
    };

    // File rows (sorted by path)
    std::vector<FileCoverage> sorted_files = report.files;
    std::sort(
        sorted_files.begin(), sorted_files.end(),
        [](const FileCoverage& a, const FileCoverage& b) { return a.file_path < b.file_path; });

    for (const auto& fc : sorted_files) {
        // Truncate long paths
        std::string display_path = fc.file_path;
        if (display_path.length() > 37) {
            display_path = "..." + display_path.substr(display_path.length() - 34);
        }

        std::ostringstream row;
        row << " " << std::left << std::setw(40) << display_path;
        row << colorize_percent(fc.line_percent()) << std::right << std::setw(6) << std::fixed
            << std::setprecision(1) << fc.line_percent() << "%" << reset;
        row << colorize_percent(fc.branch_percent()) << std::setw(6) << std::fixed
            << std::setprecision(1) << fc.branch_percent() << "%" << reset;
        row << colorize_percent(fc.function_percent()) << std::setw(6) << std::fixed
            << std::setprecision(1) << fc.function_percent() << "%" << reset;
        TML_LOG_INFO("test", row.str());
    }

    // Total row
    TML_LOG_INFO("test", " " << std::string(60, '-'));
    {
        std::ostringstream total_row;
        total_row << bold << " " << std::left << std::setw(40) << "Total" << reset;
        total_row << colorize_percent(report.summary.line_percent()) << std::right << std::setw(6)
                  << std::fixed << std::setprecision(1) << report.summary.line_percent() << "%"
                  << reset;
        total_row << colorize_percent(report.summary.branch_percent()) << std::setw(6) << std::fixed
                  << std::setprecision(1) << report.summary.branch_percent() << "%" << reset;
        total_row << colorize_percent(report.summary.function_percent()) << std::setw(6)
                  << std::fixed << std::setprecision(1) << report.summary.function_percent() << "%"
                  << reset;
        TML_LOG_INFO("test", total_row.str());
    }
    TML_LOG_INFO("test", " " << std::string(60, '-'));

    // Uncovered files
    if (!report.uncovered_files.empty()) {
        TML_LOG_INFO("test", dim << " Uncovered files:" << reset);
        for (const auto& f : report.uncovered_files) {
            TML_LOG_INFO("test", "   " << red << f << " (0%)" << reset);
        }
    }
}

CoverageReport CoverageCollector::generate_function_report(const fs::path& profdata) {
    CoverageReport report;
    report.success = false;

    if (!fs::exists(profdata)) {
        report.error_message = "Profile data file not found: " + profdata.string();
        return report;
    }

    // Build command: llvm-profdata show --all-functions profdata
    // Create temp file for output - use native paths on Windows for cmd.exe compatibility
    fs::path temp_output = fs::temp_directory_path() / "tml_function_coverage.txt";

    std::ostringstream cmd;
#ifdef _WIN32
    // On Windows, cmd.exe needs careful quoting. Use native paths for redirection.
    // Quote the entire command with redirects to handle paths with spaces correctly
    cmd << "\"\"" << profdata_path_ << "\" show --all-functions";
    cmd << " \"" << fs::absolute(profdata).string() << "\"";
    cmd << " > \"" << temp_output.string() << "\" 2>&1\"";
#else
    cmd << profdata_path_;
    cmd << " show --all-functions";
    cmd << " \"" << to_forward_slashes(profdata.string()) << "\"";
    cmd << " > \"" << to_forward_slashes(temp_output.string()) << "\" 2>&1";
#endif

    std::string command = cmd.str();
    std::system(command.c_str());

    // Parse the output
    std::ifstream infile(temp_output);
    if (!infile.is_open()) {
        report.error_message = "Failed to read function coverage output";
        return report;
    }

    std::string line;
    std::string current_func;
    int64_t total_functions = 0;
    int64_t covered_functions = 0;
    int64_t total_call_count = 0;

    // Parse llvm-profdata show output:
    // Counters:
    //   func_name:
    //     Hash: 0x...
    //     Counters: N
    //     Function count: M
    while (std::getline(infile, line)) {
        // Look for function name line (ends with colon, indented with 2 spaces)
        if (line.length() > 3 && line[0] == ' ' && line[1] == ' ' && line[2] != ' ' &&
            line.back() == ':') {
            // Extract function name (remove leading spaces and trailing colon)
            current_func = line.substr(2, line.length() - 3);
        }
        // Look for "Function count:" line
        else if (line.find("Function count:") != std::string::npos) {
            size_t pos = line.find("Function count:");
            std::string count_str = line.substr(pos + 15);
            // Trim leading spaces
            size_t start = count_str.find_first_not_of(' ');
            if (start != std::string::npos) {
                count_str = count_str.substr(start);
            }

            int64_t count = 0;
            try {
                count = std::stoll(count_str);
            } catch (...) {
                count = 0;
            }

            if (!current_func.empty()) {
                FunctionCoverage fc;
                fc.name = current_func;
                fc.call_count = count;
                report.functions.push_back(fc);

                total_functions++;
                total_call_count += count;
                if (count > 0) {
                    covered_functions++;
                } else {
                    report.uncovered_functions.push_back(current_func);
                }
            }
            current_func.clear();
        }
    }

    infile.close();
    fs::remove(temp_output);

    // Update summary
    report.summary.total_functions = static_cast<int>(total_functions);
    report.summary.covered_functions = static_cast<int>(covered_functions);

    report.success = true;
    return report;
}

// Helper to extract module name from mangled function name
// e.g., "tml_core_slice_get" -> "core/slice"
// e.g., "tml_std_sync_mutex_lock" -> "std/sync/mutex"
// e.g., "tml_s0_my_func" -> "test" (user test code)
static std::string extract_module_name(const std::string& func_name) {
    // Skip "tml_" prefix
    if (func_name.substr(0, 4) != "tml_") {
        return "other";
    }

    std::string rest = func_name.substr(4);

    // Test code: tml_s0_, tml_s1_, etc. (suite index prefix)
    if (rest.length() > 2 && rest[0] == 's' && std::isdigit(rest[1])) {
        return "tests";
    }

    // Library code: tml_core_*, tml_std_*, tml_test_*
    size_t first_underscore = rest.find('_');
    if (first_underscore == std::string::npos) {
        return "other";
    }

    std::string lib = rest.substr(0, first_underscore);
    if (lib != "core" && lib != "std" && lib != "test") {
        return "tests"; // User code without known prefix
    }

    // Extract submodule: core_slice_*, std_sync_mutex_*
    rest = rest.substr(first_underscore + 1);
    size_t second_underscore = rest.find('_');
    if (second_underscore == std::string::npos) {
        return lib;
    }

    std::string submodule = rest.substr(0, second_underscore);

    // Check for deeper nesting: std_sync_mutex_* -> std/sync/mutex
    rest = rest.substr(second_underscore + 1);
    size_t third_underscore = rest.find('_');

    // Known submodules that have further nesting
    static const std::set<std::string> nested_modules = {
        "sync", "thread", "collections", "iter", "net", "io",  "json", "ops",
        "num",  "slice",  "ascii",       "cell", "ptr", "fmt", "alloc"};

    if (nested_modules.count(submodule) && third_underscore != std::string::npos) {
        std::string subsubmodule = rest.substr(0, third_underscore);
        // Only add if it looks like a module name (not a function name part)
        if (subsubmodule.length() <= 12 &&
            subsubmodule.find_first_of("0123456789") == std::string::npos) {
            return lib + "/" + submodule + "/" + subsubmodule;
        }
    }

    return lib + "/" + submodule;
}

void CoverageCollector::print_function_report(const CoverageReport& report) {
    using namespace colors;

    if (!report.success) {
        TML_LOG_ERROR("test", report.error_message);
        return;
    }

    // Group functions by module
    struct ModuleCoverage {
        int total = 0;
        int covered = 0;
        std::vector<std::string> uncovered_funcs;
    };
    std::map<std::string, ModuleCoverage> modules;

    for (const auto& fc : report.functions) {
        std::string module = extract_module_name(fc.name);
        modules[module].total++;
        if (fc.call_count > 0) {
            modules[module].covered++;
        } else {
            modules[module].uncovered_funcs.push_back(fc.name);
        }
    }

    // Helper for colorizing
    auto colorize_percent = [](double pct) -> std::string {
        if (pct >= 80.0)
            return std::string(colors::green);
        if (pct >= 50.0)
            return std::string(colors::yellow);
        return std::string(colors::red);
    };

    auto format_bar = [](double pct, int width = 20) -> std::string {
        int filled = static_cast<int>(pct / 100.0 * width);
        std::string bar;
        for (int i = 0; i < width; i++) {
            bar += (i < filled) ? "█" : "░";
        }
        return bar;
    };

    // Header - vitest style
    TML_LOG_INFO("test", bold << " Coverage Report" << reset);
    TML_LOG_INFO("test", " " << std::string(72, '-'));
    TML_LOG_INFO("test", dim << " Module                        "
                             << "│ Funcs     │ Coverage" << reset);
    TML_LOG_INFO("test", " " << std::string(72, '-'));

    // Sort modules: library first, then tests
    std::vector<std::pair<std::string, ModuleCoverage>> sorted_modules(modules.begin(),
                                                                       modules.end());
    std::sort(sorted_modules.begin(), sorted_modules.end(), [](const auto& a, const auto& b) {
        // "tests" goes last
        if (a.first == "tests")
            return false;
        if (b.first == "tests")
            return true;
        return a.first < b.first;
    });

    int total_funcs = 0;
    int total_covered = 0;

    for (const auto& [module, cov] : sorted_modules) {
        double pct = cov.total > 0 ? (100.0 * cov.covered / cov.total) : 0.0;
        total_funcs += cov.total;
        total_covered += cov.covered;

        // Format module name (pad to 28 chars)
        std::string display_module = module;
        if (display_module.length() > 28) {
            display_module = "..." + display_module.substr(display_module.length() - 25);
        }

        std::ostringstream mod_row;
        mod_row << " " << std::left << std::setw(30) << display_module << "│ " << std::right
                << std::setw(4) << cov.covered << "/" << std::left << std::setw(4) << cov.total
                << " │ " << colorize_percent(pct) << std::right << std::setw(5) << std::fixed
                << std::setprecision(1) << pct << "%" << reset << " " << dim << format_bar(pct, 15)
                << reset;
        TML_LOG_INFO("test", mod_row.str());
    }

    // Total line
    double total_pct = total_funcs > 0 ? (100.0 * total_covered / total_funcs) : 0.0;
    TML_LOG_INFO("test", " " << std::string(72, '-'));
    {
        std::ostringstream total_row;
        total_row << bold << " " << std::left << std::setw(30) << "Total"
                  << "│ " << std::right << std::setw(4) << total_covered << "/" << std::left
                  << std::setw(4) << total_funcs << " │ " << colorize_percent(total_pct)
                  << std::right << std::setw(5) << std::fixed << std::setprecision(1) << total_pct
                  << "%" << reset << " " << dim << format_bar(total_pct, 15) << reset;
        TML_LOG_INFO("test", total_row.str());
    }
    TML_LOG_INFO("test", " " << std::string(72, '-'));

    // Show modules with low coverage (< 50%)
    std::vector<std::pair<std::string, ModuleCoverage>> low_coverage;
    for (const auto& [module, cov] : sorted_modules) {
        if (module == "tests")
            continue; // Skip test code
        double pct = cov.total > 0 ? (100.0 * cov.covered / cov.total) : 0.0;
        if (pct < 50.0 && cov.total > 0) {
            low_coverage.push_back({module, cov});
        }
    }

    if (!low_coverage.empty()) {
        TML_LOG_INFO("test", yellow << bold << " Low Coverage Modules:" << reset);
        for (const auto& [module, cov] : low_coverage) {
            double pct = 100.0 * cov.covered / cov.total;
            TML_LOG_INFO("test", "   " << red << module << reset << " - " << cov.covered << "/"
                                       << cov.total << " (" << std::fixed << std::setprecision(0)
                                       << pct << "%)");

            // Show up to 5 uncovered functions per module
            int shown = 0;
            for (const auto& fn : cov.uncovered_funcs) {
                if (shown >= 5) {
                    TML_LOG_INFO("test", dim << "      ... and " << (cov.uncovered_funcs.size() - 5)
                                             << " more" << reset);
                    break;
                }
                // Extract just function name from mangled name
                std::string short_name = fn;
                size_t last_underscore = fn.rfind('_');
                if (last_underscore != std::string::npos && last_underscore > 4) {
                    // Try to get a reasonable function name
                    short_name = fn.substr(fn.find(module.substr(module.rfind('/') + 1)) +
                                           module.substr(module.rfind('/') + 1).length() + 1);
                    if (short_name.empty() || short_name[0] == '_') {
                        short_name = fn;
                    }
                }
                TML_LOG_INFO("test", dim << "      - " << short_name << reset);
                shown++;
            }
        }
    }

    TML_LOG_INFO("test", " " << std::string(60, '-'));
}

} // namespace tml::cli::tester
