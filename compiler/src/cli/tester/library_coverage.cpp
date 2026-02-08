//! # Library Coverage Analysis
//!
//! Scans library source files to find all function definitions,
//! then compares against runtime coverage data to report what's NOT covered.

#include "log/log.hpp"
#include "tester_internal.hpp"

#include <fstream>
#include <regex>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace tml::cli::tester {

// ============================================================================
// Library Function Scanner
// ============================================================================

struct ModuleCoverage {
    std::string name;
    std::vector<std::string> functions;
    std::vector<std::string> covered_functions;   // Functions that were tested (unique)
    std::vector<std::string> uncovered_functions; // Functions that were NOT tested (unique)
    int covered_count = 0;

    // Deduplicate functions
    void deduplicate() {
        std::set<std::string> seen;
        std::vector<std::string> unique_funcs;
        for (const auto& f : functions) {
            if (seen.insert(f).second) {
                unique_funcs.push_back(f);
            }
        }
        functions = std::move(unique_funcs);
    }
};

/// Extract function names from a TML source file
static std::vector<std::string> extract_functions(const fs::path& file) {
    std::vector<std::string> functions;
    std::string current_impl;
    int impl_brace_depth = 0; // Track brace depth inside impl blocks

    std::ifstream ifs(file);
    if (!ifs.is_open()) {
        return functions;
    }

    std::string line;

    // Match impl blocks with optional generic parameters:
    // - impl TypeName              → captures TypeName
    // - impl[T] TypeName           → captures TypeName
    // - impl[T] TypeName[T]        → captures TypeName
    // - impl[T] Behavior for Type  → captures Type (not Behavior!)
    // - impl[T] Drop for Arc[T]    → captures Arc
    //
    // The regex has two capture groups:
    // - Group 1: First type name after impl (could be behavior or type)
    // - Group 2: Type name after "for" keyword (when implementing behavior)
    // We use group 2 if present, otherwise group 1.
    std::regex impl_regex(R"(^\s*impl\s*(?:\[[^\]]*\])?\s*(\w+)(?:\s+for\s+(\w+))?)");

    // Match behavior blocks with optional generic parameters:
    // - behavior BehaviorName
    // - behavior BehaviorName[T]
    // - pub behavior BehaviorName[Rhs = Self]
    std::regex behavior_regex(R"(^\s*(pub\s+)?behavior\s+(\w+))");

    std::regex func_regex(R"(^\s*(pub\s+)?func\s+(\w+))");
    std::regex extern_regex(R"(@extern\()");
    std::smatch match;
    std::string prev_line; // Track previous line for @extern detection

    while (std::getline(ifs, line)) {
        // Track impl blocks - detect "impl TypeName" or "impl[T] TypeName" or "impl Behavior for
        // Type"
        if (std::regex_search(line, match, impl_regex)) {
            // If group 2 matched (has "for Type"), use the type after "for"
            // Unless it's a single-letter generic like T, in which case use the behavior name
            // Otherwise use group 1 (the first type name)
            if (match[2].matched) {
                std::string type_after_for = match[2].str();
                // If the type is a single uppercase letter (generic parameter like T, U, V),
                // use the behavior name (group 1) instead for coverage matching
                if (type_after_for.size() == 1 && std::isupper(type_after_for[0])) {
                    current_impl =
                        match[1].str(); // Behavior name (e.g., Borrow from "impl Borrow for T")
                } else {
                    current_impl =
                        type_after_for; // Concrete type (e.g., Arc from "impl Drop for Arc")
                }
            } else {
                current_impl = match[1].str(); // Direct impl (e.g., Arc from "impl Arc")
            }
            impl_brace_depth = 0; // Reset depth, will count opening brace below
        }
        // Track behavior blocks - detect "behavior BehaviorName" or "pub behavior BehaviorName"
        else if (std::regex_search(line, match, behavior_regex)) {
            current_impl = match[2].str();
            impl_brace_depth = 0; // Reset depth, will count opening brace below
        }

        // Count braces to track impl/behavior block scope
        if (!current_impl.empty()) {
            for (char c : line) {
                if (c == '{')
                    impl_brace_depth++;
                else if (c == '}')
                    impl_brace_depth--;
            }
            // When we exit the impl/behavior block (depth <= 0), clear current_impl
            if (impl_brace_depth <= 0) {
                current_impl.clear();
                impl_brace_depth = 0;
            }
        }

        // Match function definitions
        if (std::regex_search(line, match, func_regex)) {
            std::string func_name = match[2].str();

            // Skip test functions
            if (func_name.rfind("test_", 0) == 0) {
                prev_line = line;
                continue;
            }

            // Skip @extern FFI declarations (check previous line for @extern annotation)
            // These are just declarations that map to C functions, no TML code to cover
            if (std::regex_search(prev_line, extern_regex)) {
                prev_line = line;
                continue;
            }

            // Skip ffi_ prefixed functions (convention for FFI wrappers)
            if (func_name.rfind("ffi_", 0) == 0) {
                prev_line = line;
                continue;
            }

            if (!current_impl.empty()) {
                // Use :: separator to match codegen coverage tracking
                functions.push_back(current_impl + "::" + func_name);
            } else {
                functions.push_back(func_name);
            }
        }

        prev_line = line;
    }

    return functions;
}

/// Get module name from file path
static std::string get_module_name(const fs::path& file, const fs::path& base) {
    auto rel = fs::relative(file, base);
    std::string result = rel.string();

    // Normalize path separators
    std::replace(result.begin(), result.end(), '\\', '/');

    // Remove /src/ prefix if present
    size_t src_pos = result.find("/src/");
    if (src_pos != std::string::npos) {
        result = result.substr(src_pos + 5);
    } else if (result.rfind("src/", 0) == 0) {
        result = result.substr(4);
    }

    // Remove .tml extension
    if (result.size() > 4 && result.substr(result.size() - 4) == ".tml") {
        result = result.substr(0, result.size() - 4);
    }

    // Remove /mod suffix
    if (result.size() > 4 && result.substr(result.size() - 4) == "/mod") {
        result = result.substr(0, result.size() - 4);
    }

    return result;
}

/// Scan library directories for all functions
static std::vector<ModuleCoverage> scan_library(const std::vector<fs::path>& lib_dirs) {
    std::unordered_map<std::string, std::vector<std::string>> modules;

    for (const auto& lib_dir : lib_dirs) {
        if (!fs::exists(lib_dir)) {
            continue;
        }

        for (const auto& entry : fs::recursive_directory_iterator(lib_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string filename = entry.path().filename().string();

            // Skip test files
            if (filename.find(".test.tml") != std::string::npos) {
                continue;
            }

            // Skip non-TML files
            if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".tml") {
                continue;
            }

            // Skip tests directory
            std::string path_str = entry.path().string();
            std::replace(path_str.begin(), path_str.end(), '\\', '/');
            if (path_str.find("/tests/") != std::string::npos) {
                continue;
            }

            auto funcs = extract_functions(entry.path());
            if (!funcs.empty()) {
                std::string module = get_module_name(entry.path(), lib_dir);
                auto& mod_funcs = modules[module];
                mod_funcs.insert(mod_funcs.end(), funcs.begin(), funcs.end());
            }
        }
    }

    // Convert to sorted vector
    std::vector<ModuleCoverage> result;
    for (auto& [name, funcs] : modules) {
        ModuleCoverage mc;
        mc.name = name;
        mc.functions = std::move(funcs);
        mc.deduplicate(); // Remove duplicate function names
        result.push_back(std::move(mc));
    }

    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });

    return result;
}

// ============================================================================
// Coverage Report Generation
// ============================================================================

void print_library_coverage_report(const std::set<std::string>& covered_functions,
                                   const ColorOutput& c, const TestRunStats& test_stats) {
    // Suppress unused parameter warning if test_stats is empty
    (void)test_stats;

    // Get library paths (core, std, and test libraries)
    fs::path cwd = fs::current_path();
    std::vector<fs::path> lib_dirs = {cwd / "lib" / "core", cwd / "lib" / "std",
                                      cwd / "lib" / "test"};

    // Scan library
    auto modules = scan_library(lib_dirs);

    if (modules.empty()) {
        return; // No library found
    }

    // Add builtin methods that we track (using :: separator to match codegen)
    ModuleCoverage builtins;
    builtins.name = "builtins";
    builtins.functions = {"Slice::len",         "Slice::is_empty",  "MutSlice::len",
                          "MutSlice::is_empty", "Array::len",       "Array::is_empty",
                          "Array::get",         "Array::first",     "Array::last",
                          "Array::map",         "Array::eq",        "Array::ne",
                          "Array::cmp",         "Maybe::is_just",   "Maybe::is_nothing",
                          "Maybe::unwrap",      "Maybe::unwrap_or", "Maybe::map"};
    modules.insert(modules.begin(), std::move(builtins));

    // Calculate coverage and populate per-function lists
    int total_funcs = 0;
    int total_covered = 0;
    std::vector<std::pair<std::string, std::vector<std::string>>> uncovered_by_module;

    for (auto& mod : modules) {
        mod.covered_functions.clear();
        mod.uncovered_functions.clear();
        for (const auto& func : mod.functions) {
            total_funcs++;
            if (covered_functions.count(func) > 0) {
                mod.covered_count++;
                mod.covered_functions.push_back(func);
                total_covered++;
            } else {
                mod.uncovered_functions.push_back(func);
            }
        }
        if (!mod.uncovered_functions.empty()) {
            uncovered_by_module.push_back({mod.name, mod.uncovered_functions});
        }
    }

    // Print report
    TML_LOG_INFO("test", c.cyan() << c.bold()
                                  << "================================================================================"
                                  << c.reset());
    TML_LOG_INFO("test", c.cyan() << c.bold() << "                    LIBRARY COVERAGE ANALYSIS"
                                  << c.reset());
    TML_LOG_INFO("test", c.cyan() << c.bold()
                                  << "================================================================================"
                                  << c.reset());

    // Overall summary
    double overall_pct = total_funcs > 0 ? (100.0 * total_covered / total_funcs) : 0.0;
    {
        std::ostringstream oss;
        oss << " Library Coverage: " << c.bold() << total_covered << "/" << total_funcs
            << c.reset() << " functions (" << c.bold();
        if (overall_pct < 10)
            oss << c.red();
        else if (overall_pct < 50)
            oss << c.yellow();
        else
            oss << c.green();
        oss << std::fixed << std::setprecision(1) << overall_pct << "%" << c.reset() << ")";
        TML_LOG_INFO("test", oss.str());
    }
    TML_LOG_INFO("test", " Total Functions Called: " << c.green() << c.bold()
                                                     << covered_functions.size() << c.reset());

    // Per-module table with function details
    TML_LOG_INFO("test", c.dim()
                            << "--------------------------------------------------------------------------------"
                            << c.reset());
    {
        std::ostringstream hdr;
        hdr << " " << std::left << std::setw(45) << "Module" << std::right << std::setw(12)
            << "Coverage" << std::setw(10) << "Percent";
        TML_LOG_INFO("test", hdr.str());
    }
    TML_LOG_INFO("test", c.dim()
                            << "--------------------------------------------------------------------------------"
                            << c.reset());

    for (const auto& mod : modules) {
        int mod_total = static_cast<int>(mod.functions.size());
        double pct = mod_total > 0 ? (100.0 * mod.covered_count / mod_total) : 0.0;

        // Status symbol
        const char* status;
        const char* color;
        if (pct == 100.0) {
            status = "+";
            color = c.green();
        } else if (pct == 0.0) {
            status = "X";
            color = c.red();
        } else {
            status = "~";
            color = c.yellow();
        }

        {
            std::ostringstream mod_row;
            mod_row << " " << color << status << c.reset() << " " << std::left << std::setw(43)
                    << mod.name << std::right << std::setw(5) << mod.covered_count << "/" << std::left
                    << std::setw(5) << mod_total << std::right << color << std::setw(9) << std::fixed
                    << std::setprecision(1) << pct << "%" << c.reset();
            TML_LOG_INFO("test", mod_row.str());
        }

        // Always show function details for every module
        for (const auto& func : mod.covered_functions) {
            TML_LOG_INFO("test",
                         "      " << c.green() << "+" << c.reset() << " " << c.dim() << func << c.reset());
        }
        for (const auto& func : mod.uncovered_functions) {
            TML_LOG_INFO("test",
                         "      " << c.red() << "X" << c.reset() << " " << c.dim() << func << c.reset());
        }
    }

    // Count modules with 0% coverage
    int zero_coverage_modules = 0;
    for (const auto& mod : modules) {
        if (mod.covered_count == 0 && !mod.functions.empty()) {
            zero_coverage_modules++;
        }
    }

    TML_LOG_INFO("test", c.dim()
                            << "--------------------------------------------------------------------------------"
                            << c.reset());
    TML_LOG_INFO("test", " " << c.red() << c.bold() << zero_coverage_modules << c.reset()
                             << " modules with 0% coverage");
    TML_LOG_INFO("test", c.dim()
                            << "================================================================================"
                            << c.reset());

    // Priority table - modules that need tests
    TML_LOG_INFO("test", c.cyan() << c.bold()
                                  << "================================================================================"
                                  << c.reset());
    TML_LOG_INFO("test", c.cyan() << c.bold() << "                    TEST IMPROVEMENT PRIORITIES"
                                  << c.reset());
    TML_LOG_INFO("test", c.cyan() << c.bold()
                                  << "================================================================================"
                                  << c.reset());

    // Collect and sort by priority (0% with most functions first, then low coverage)
    struct PriorityModule {
        std::string name;
        int total;
        int covered;
        double pct;
        bool is_critical; // sync/mutex, Arc, hash, fmt are critical
    };

    std::vector<PriorityModule> priority_list;
    std::set<std::string> critical_modules = {
        "sync/mutex",    "sync/Arc",   "sync/rwlock", "sync/queue", "sync/stack",
        "hash",          "intrinsics", "num/integer", "ops/bit",    "fmt/impls",
        "fmt/formatter", "convert",    "error",       "json",       "pool"};

    for (const auto& mod : modules) {
        if (mod.functions.empty())
            continue;
        int mod_total = static_cast<int>(mod.functions.size());
        double pct = 100.0 * mod.covered_count / mod_total;

        // Only include modules that need improvement (<50% coverage)
        if (pct < 50.0) {
            PriorityModule pm;
            pm.name = mod.name;
            pm.total = mod_total;
            pm.covered = mod.covered_count;
            pm.pct = pct;
            pm.is_critical = critical_modules.count(mod.name) > 0;
            priority_list.push_back(pm);
        }
    }

    // Sort: critical first, then by uncovered count (descending)
    std::sort(priority_list.begin(), priority_list.end(),
              [](const PriorityModule& a, const PriorityModule& b) {
                  if (a.is_critical != b.is_critical)
                      return a.is_critical > b.is_critical;
                  int uncov_a = a.total - a.covered;
                  int uncov_b = b.total - b.covered;
                  return uncov_a > uncov_b;
              });

    // Print critical modules (0% that are important)
    TML_LOG_INFO("test", " " << c.red() << c.bold() << "CRITICAL (0% - high priority):" << c.reset());
    TML_LOG_INFO("test", c.dim()
                            << " -----------------------------------------------------------------------"
                            << c.reset());

    int critical_count = 0;
    for (const auto& pm : priority_list) {
        if (pm.pct == 0.0 && pm.is_critical && critical_count < 10) {
            std::ostringstream row;
            row << "  " << c.red() << "•" << c.reset() << " " << std::left << std::setw(35)
                << pm.name << std::right << std::setw(4) << pm.covered << "/" << std::left
                << std::setw(4) << pm.total << " " << c.red() << "HIGH PRIORITY" << c.reset();
            TML_LOG_INFO("test", row.str());
            critical_count++;
        }
    }
    if (critical_count == 0) {
        TML_LOG_INFO("test", "  " << c.dim() << "(none)" << c.reset());
    }

    // Print 0% coverage modules with most functions
    TML_LOG_INFO("test",
                 " " << c.red() << c.bold() << "ZERO COVERAGE (0% - most functions):" << c.reset());
    TML_LOG_INFO("test", c.dim()
                            << " -----------------------------------------------------------------------"
                            << c.reset());

    int zero_count = 0;
    for (const auto& pm : priority_list) {
        if (pm.pct == 0.0 && !pm.is_critical && zero_count < 15) {
            std::ostringstream row;
            row << "  " << c.red() << "•" << c.reset() << " " << std::left << std::setw(35)
                << pm.name << std::right << std::setw(4) << pm.covered << "/" << std::left
                << std::setw(4) << pm.total << " (" << (pm.total - pm.covered) << " missing)";
            TML_LOG_INFO("test", row.str());
            zero_count++;
        }
    }

    // Print low coverage modules
    TML_LOG_INFO("test",
                 " " << c.yellow() << c.bold() << "LOW COVERAGE (<30%):" << c.reset());
    TML_LOG_INFO("test", c.dim()
                            << " -----------------------------------------------------------------------"
                            << c.reset());

    int low_count = 0;
    for (const auto& pm : priority_list) {
        if (pm.pct > 0.0 && pm.pct < 30.0 && low_count < 15) {
            std::ostringstream row;
            row << "  " << c.yellow() << "~" << c.reset() << " " << std::left << std::setw(35)
                << pm.name << std::right << std::setw(4) << pm.covered << "/" << std::left
                << std::setw(4) << pm.total << " " << c.yellow() << std::fixed
                << std::setprecision(1) << pm.pct << "%" << c.reset();
            TML_LOG_INFO("test", row.str());
            low_count++;
        }
    }

    TML_LOG_INFO("test", c.dim()
                            << "================================================================================"
                            << c.reset());

    // Print uncovered functions by module
    if (!uncovered_by_module.empty()) {
        TML_LOG_INFO("test", c.cyan() << c.bold()
                                      << "================================================================================"
                                      << c.reset());
        TML_LOG_INFO("test", c.cyan() << c.bold()
                                      << "                    UNCOVERED FUNCTIONS BY MODULE" << c.reset());
        TML_LOG_INFO("test", c.cyan() << c.bold()
                                      << "================================================================================"
                                      << c.reset());

        // Sort by number of uncovered functions (most first)
        std::vector<std::pair<std::string, std::vector<std::string>>> sorted_uncovered =
            uncovered_by_module;
        std::sort(sorted_uncovered.begin(), sorted_uncovered.end(),
                  [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });

        // Only show top 20 modules with most uncovered functions
        int shown_modules = 0;
        for (const auto& [module_name, funcs] : sorted_uncovered) {
            if (shown_modules >= 20)
                break;
            shown_modules++;

            TML_LOG_INFO("test", " " << c.yellow() << c.bold() << module_name << c.reset() << " "
                                     << c.dim() << "(" << funcs.size() << " uncovered)" << c.reset());

            // Show up to 10 functions per module
            int shown_funcs = 0;
            for (const auto& func : funcs) {
                if (shown_funcs >= 10) {
                    TML_LOG_INFO("test", "   " << c.dim() << "... and " << (funcs.size() - 10)
                                               << " more" << c.reset());
                    break;
                }
                TML_LOG_INFO("test",
                             "   " << c.red() << "✗" << c.reset() << " " << c.dim() << func << c.reset());
                shown_funcs++;
            }
        }

        if (sorted_uncovered.size() > 20) {
            TML_LOG_INFO("test", " " << c.dim() << "... and " << (sorted_uncovered.size() - 20)
                                     << " more modules with uncovered functions" << c.reset());
        }

        TML_LOG_INFO("test", c.dim()
                                << "================================================================================"
                                << c.reset());
    }
}

// ============================================================================
// HTML Report Generation
// ============================================================================

void write_library_coverage_html(const std::set<std::string>& covered_functions,
                                 const std::string& output_path, const TestRunStats& test_stats) {
    // Get library paths (core, std, and test libraries)
    fs::path cwd = fs::current_path();
    std::vector<fs::path> lib_dirs = {cwd / "lib" / "core", cwd / "lib" / "std",
                                      cwd / "lib" / "test"};

    // Scan library
    auto modules = scan_library(lib_dirs);

    if (modules.empty()) {
        return;
    }

    // Add builtin methods (using :: separator to match codegen)
    ModuleCoverage builtins;
    builtins.name = "builtins";
    builtins.functions = {"Slice::len",         "Slice::is_empty",  "MutSlice::len",
                          "MutSlice::is_empty", "Array::len",       "Array::is_empty",
                          "Array::get",         "Array::first",     "Array::last",
                          "Array::map",         "Array::eq",        "Array::ne",
                          "Array::cmp",         "Maybe::is_just",   "Maybe::is_nothing",
                          "Maybe::unwrap",      "Maybe::unwrap_or", "Maybe::map"};
    modules.insert(modules.begin(), std::move(builtins));

    // Calculate coverage and populate per-function lists
    int total_funcs = 0;
    int total_covered = 0;
    std::vector<std::pair<std::string, std::vector<std::string>>> uncovered_by_module;

    for (auto& mod : modules) {
        mod.covered_functions.clear();
        mod.uncovered_functions.clear();
        for (const auto& func : mod.functions) {
            total_funcs++;
            if (covered_functions.count(func) > 0) {
                mod.covered_count++;
                mod.covered_functions.push_back(func);
                total_covered++;
            } else {
                mod.uncovered_functions.push_back(func);
            }
        }
        if (!mod.uncovered_functions.empty()) {
            uncovered_by_module.push_back({mod.name, mod.uncovered_functions});
        }
    }

    double overall_pct = total_funcs > 0 ? (100.0 * total_covered / total_funcs) : 0.0;

    // Calculate test stats for all suites
    int tml_tests = 0, tml_suites = 0;
    for (const auto& suite : test_stats.suites) {
        tml_tests += suite.test_count;
        tml_suites++;
    }
    int tml_files = test_stats.total_files;

    // Count modules by coverage status
    int full_coverage = 0, partial_coverage = 0, zero_coverage = 0;
    for (const auto& mod : modules) {
        if (mod.functions.empty())
            continue;
        if (mod.covered_count == 0)
            zero_coverage++;
        else if (mod.covered_count == static_cast<int>(mod.functions.size()))
            full_coverage++;
        else
            partial_coverage++;
    }

    // Open file
    std::ofstream f(output_path);
    if (!f.is_open()) {
        TML_LOG_ERROR("test", "Cannot write coverage HTML to " << output_path);
        return;
    }

    // Write HTML
    f << R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TML Library Coverage Report</title>
  <style>
    :root {
      --bg: #0d1117;
      --surface: #161b22;
      --border: #30363d;
      --text: #c9d1d9;
      --text-dim: #8b949e;
      --green: #3fb950;
      --yellow: #d29922;
      --red: #f85149;
      --blue: #58a6ff;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;
      background: var(--bg);
      color: var(--text);
      line-height: 1.5;
      padding: 24px;
    }
    .container { max-width: 1200px; margin: 0 auto; }
    h1 { font-size: 24px; margin-bottom: 8px; }
    .subtitle { color: var(--text-dim); margin-bottom: 24px; }

    /* Stats cards */
    .stats {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
      gap: 12px;
      margin-bottom: 24px;
    }
    .stats-main {
      grid-template-columns: repeat(5, 1fr);
    }
    .stats-secondary {
      grid-template-columns: repeat(3, 1fr);
      max-width: 600px;
    }
    @media (max-width: 900px) {
      .stats-main { grid-template-columns: repeat(3, 1fr); }
    }
    @media (max-width: 600px) {
      .stats-main { grid-template-columns: repeat(2, 1fr); }
      .stats-secondary { grid-template-columns: repeat(2, 1fr); }
    }
    .stat-card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 16px;
      min-width: 0;
    }
    .stat-value { font-size: 28px; font-weight: 600; white-space: nowrap; }
    .stat-label { color: var(--text-dim); font-size: 11px; margin-top: 4px; }
    .stat-green { color: var(--green); }
    .stat-yellow { color: var(--yellow); }
    .stat-red { color: var(--red); }

    /* Progress bar */
    .progress-container { margin-bottom: 24px; }
    .progress-bar {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 6px;
      height: 24px;
      overflow: hidden;
    }
    .progress-fill {
      height: 100%;
      transition: width 0.3s;
    }
    .progress-text {
      text-align: center;
      margin-top: 8px;
      color: var(--text-dim);
      font-size: 14px;
    }

    /* Module table */
    .section-title {
      font-size: 18px;
      margin: 24px 0 16px;
      padding-bottom: 8px;
      border-bottom: 1px solid var(--border);
    }
    table {
      width: 100%;
      border-collapse: collapse;
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 6px;
      overflow: hidden;
      font-size: 14px;
    }
    th, td {
      padding: 12px 16px;
      text-align: left;
      border-bottom: 1px solid var(--border);
    }
    th {
      background: var(--bg);
      font-weight: 600;
      color: var(--text-dim);
      font-size: 12px;
      text-transform: uppercase;
    }
    tr:last-child td { border-bottom: none; }
    tr:hover { background: rgba(88, 166, 255, 0.05); }

    .module-name { font-family: monospace; }
    .coverage-bar {
      width: 120px;
      height: 8px;
      background: var(--border);
      border-radius: 4px;
      overflow: hidden;
      display: inline-block;
      vertical-align: middle;
      margin-right: 8px;
    }
    .coverage-bar-fill { height: 100%; }
    .coverage-green { background: var(--green); }
    .coverage-yellow { background: var(--yellow); }
    .coverage-red { background: var(--red); }

    .status-badge {
      display: inline-block;
      padding: 2px 8px;
      border-radius: 12px;
      font-size: 12px;
      font-weight: 500;
    }
    .badge-full { background: rgba(63, 185, 80, 0.2); color: var(--green); }
    .badge-partial { background: rgba(210, 153, 34, 0.2); color: var(--yellow); }
    .badge-none { background: rgba(248, 81, 73, 0.2); color: var(--red); }

    /* Uncovered section */
    .uncovered-section {
      margin-top: 32px;
    }
    .uncovered-module {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 6px;
      margin-bottom: 16px;
      overflow: hidden;
    }
    .uncovered-header {
      padding: 12px 16px;
      background: var(--bg);
      border-bottom: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
      align-items: center;
      cursor: pointer;
    }
    .uncovered-header:hover { background: rgba(88, 166, 255, 0.05); }
    .uncovered-count {
      color: var(--red);
      font-size: 12px;
    }
    .uncovered-list {
      padding: 12px 16px;
      display: none;
    }
    .uncovered-module.expanded .uncovered-list { display: block; }
    .uncovered-func {
      font-family: monospace;
      font-size: 13px;
      padding: 4px 0;
      color: var(--text-dim);
    }
    .uncovered-func::before {
      content: "✗ ";
      color: var(--red);
    }

    /* Footer */
    .footer {
      margin-top: 32px;
      padding-top: 16px;
      border-top: 1px solid var(--border);
      color: var(--text-dim);
      font-size: 12px;
      text-align: center;
    }

    /* Module groups (accordion) */
    .module-groups {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .module-group {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 6px;
      overflow: hidden;
    }
    .group-header {
      padding: 12px 16px;
      background: var(--bg);
      display: flex;
      justify-content: space-between;
      align-items: center;
      cursor: pointer;
      user-select: none;
    }
    .group-header:hover { background: rgba(88, 166, 255, 0.08); }
    .group-title {
      font-family: monospace;
      font-weight: 600;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .group-title::before {
      content: "▶";
      font-size: 10px;
      transition: transform 0.2s;
    }
    .module-group.expanded .group-title::before {
      transform: rotate(90deg);
    }
    .group-stats {
      display: flex;
      align-items: center;
      gap: 16px;
      font-size: 13px;
    }
    .group-coverage {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .group-bar {
      width: 80px;
      height: 6px;
      background: var(--border);
      border-radius: 3px;
      overflow: hidden;
    }
    .group-bar-fill { height: 100%; }
    .group-content {
      display: none;
      border-top: 1px solid var(--border);
    }
    .module-group.expanded .group-content { display: block; }
    .submodule-row {
      padding: 8px 16px 8px 32px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-bottom: 1px solid var(--border);
      font-size: 13px;
    }
    .submodule-row:last-child { border-bottom: none; }
    .submodule-row:hover { background: rgba(88, 166, 255, 0.05); }
    .submodule-name {
      font-family: monospace;
      color: var(--text-dim);
    }
    .submodule-stats {
      display: flex;
      align-items: center;
      gap: 12px;
    }

    /* Tabs */
    .tabs {
      display: flex;
      gap: 4px;
      margin-bottom: 24px;
      border-bottom: 1px solid var(--border);
      padding-bottom: 0;
    }
    .tab {
      padding: 12px 20px;
      background: transparent;
      border: none;
      color: var(--text-dim);
      cursor: pointer;
      font-size: 14px;
      font-weight: 500;
      border-bottom: 2px solid transparent;
      margin-bottom: -1px;
      transition: all 0.2s;
    }
    .tab:hover {
      color: var(--text);
      background: rgba(88, 166, 255, 0.05);
    }
    .tab.active {
      color: var(--blue);
      border-bottom-color: var(--blue);
    }
    .tab-panel {
      display: none;
    }
    .tab-panel.active {
      display: block;
    }

    /* Test suites */
    .suite-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .suite-item {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .suite-name {
      font-weight: 600;
      font-family: monospace;
    }
    .suite-stats {
      display: flex;
      gap: 16px;
      align-items: center;
      color: var(--text-dim);
      font-size: 13px;
    }
    .suite-tests {
      color: var(--green);
      font-weight: 500;
    }
    .suite-duration {
      color: var(--text-dim);
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>TML Library Coverage Report</h1>
    <p class="subtitle">Generated by TML Test Runner</p>

    <div class="stats stats-main">
      <div class="stat-card">
        <div class="stat-value)"
      << (overall_pct < 10 ? " stat-red" : (overall_pct < 50 ? " stat-yellow" : " stat-green"))
      << R"(">)" << std::fixed << std::setprecision(1) << overall_pct << R"(%</div>
        <div class="stat-label">Overall Coverage</div>
      </div>
      <div class="stat-card">
        <div class="stat-value">)"
      << total_covered << " / " << total_funcs << R"(</div>
        <div class="stat-label">Library Functions Covered</div>
      </div>
      <div class="stat-card">
        <div class="stat-value stat-green">)"
      << covered_functions.size() << R"(</div>
        <div class="stat-label">Total Functions Called</div>
      </div>
      <div class="stat-card">
        <div class="stat-value stat-green">)"
      << tml_tests << R"(</div>
        <div class="stat-label">Tests Passed</div>
      </div>
      <div class="stat-card">
        <div class="stat-value">)"
      << tml_files << R"(</div>
        <div class="stat-label">Test Files</div>
      </div>
    </div>

    <div class="progress-container">
      <div class="progress-bar">
        <div class="progress-fill)"
      << (overall_pct < 10 ? " coverage-red"
                           : (overall_pct < 50 ? " coverage-yellow" : " coverage-green"))
      << R"(" style="width: )" << overall_pct << R"(%;"></div>
      </div>
      <div class="progress-text">)"
      << total_covered << " of " << total_funcs
      << R"HTML( library functions have test coverage</div>
    </div>

    <div class="tabs">
      <button class="tab active" onclick="showTab('overview')">Overview</button>
      <button class="tab" onclick="showTab('modules')">Module Coverage</button>
      <button class="tab" onclick="showTab('priorities')">Priorities</button>
      <button class="tab" onclick="showTab('uncovered')">Uncovered Functions</button>
      <button class="tab" onclick="showTab('suites')">Test Suites</button>
    </div>

    <div id="overview" class="tab-panel active">
      <div class="stats stats-secondary" style="margin-top: 0;">
        <div class="stat-card">
          <div class="stat-value stat-green">)HTML"
      << full_coverage << R"(</div>
          <div class="stat-label">Modules 100% Covered</div>
        </div>
        <div class="stat-card">
          <div class="stat-value stat-yellow">)"
      << partial_coverage << R"(</div>
          <div class="stat-label">Modules Partial</div>
        </div>
        <div class="stat-card">
          <div class="stat-value stat-red">)"
      << zero_coverage << R"(</div>
          <div class="stat-label">Modules 0% Covered</div>
        </div>
      </div>

      <h2 class="section-title">Test Suites</h2>
      <div class="suite-list">
)";

    // Write test suite details in overview
    for (const auto& suite : test_stats.suites) {
        f << "        <div class=\"suite-item\">\n";
        f << "          <span class=\"suite-name\">" << suite.name << "</span>\n";
        f << "          <div class=\"suite-stats\">\n";
        f << "            <span class=\"suite-tests\">" << suite.test_count << " tests</span>\n";
        f << "            <span class=\"suite-duration\">" << suite.duration_ms << "ms</span>\n";
        f << "          </div>\n";
        f << "        </div>\n";
    }

    f << R"(      </div>
    </div>

    <!-- Modules Tab -->
    <div id="modules" class="tab-panel">
      <h2 class="section-title">Module Coverage</h2>
      <div class="module-groups">
)";

    // Group modules by top-level category
    struct GroupStats {
        std::string name;
        int total_funcs = 0;
        int covered_funcs = 0;
        std::vector<const ModuleCoverage*> submodules;
    };
    std::map<std::string, GroupStats> groups;

    for (const auto& mod : modules) {
        // Extract group name (first segment before /)
        std::string group_name = mod.name;
        size_t slash_pos = mod.name.find('/');
        if (slash_pos != std::string::npos) {
            group_name = mod.name.substr(0, slash_pos);
        }

        auto& group = groups[group_name];
        group.name = group_name;
        group.total_funcs += static_cast<int>(mod.functions.size());
        group.covered_funcs += mod.covered_count;
        group.submodules.push_back(&mod);
    }

    // Sort groups by coverage percentage (lowest first)
    std::vector<std::pair<std::string, GroupStats*>> sorted_groups;
    for (auto& [name, stats] : groups) {
        sorted_groups.push_back({name, &stats});
    }
    std::sort(sorted_groups.begin(), sorted_groups.end(), [](const auto& a, const auto& b) {
        double pct_a = a.second->total_funcs > 0
                           ? (100.0 * a.second->covered_funcs / a.second->total_funcs)
                           : 0;
        double pct_b = b.second->total_funcs > 0
                           ? (100.0 * b.second->covered_funcs / b.second->total_funcs)
                           : 0;
        return pct_a < pct_b;
    });

    // Generate accordion groups
    for (const auto& [name, stats] : sorted_groups) {
        double group_pct =
            stats->total_funcs > 0 ? (100.0 * stats->covered_funcs / stats->total_funcs) : 0;
        std::string color_class = group_pct < 10
                                      ? "coverage-red"
                                      : (group_pct < 50 ? "coverage-yellow" : "coverage-green");
        std::string badge_class =
            group_pct == 100.0 ? "badge-full" : (group_pct == 0.0 ? "badge-none" : "badge-partial");

        f << "      <div class=\"module-group\" onclick=\"this.classList.toggle('expanded')\">\n";
        f << "        <div class=\"group-header\">\n";
        f << "          <span class=\"group-title\">" << name << "/</span>\n";
        f << "          <div class=\"group-stats\">\n";
        f << "            <div class=\"group-coverage\">\n";
        f << "              <div class=\"group-bar\"><div class=\"group-bar-fill " << color_class
          << "\" style=\"width: " << group_pct << "%;\"></div></div>\n";
        f << "              <span style=\"color: var(--"
          << (group_pct < 10 ? "red" : (group_pct < 50 ? "yellow" : "green")) << ");\">"
          << std::fixed << std::setprecision(1) << group_pct << "%</span>\n";
        f << "            </div>\n";
        f << "            <span>" << stats->covered_funcs << "/" << stats->total_funcs
          << "</span>\n";
        f << "            <span class=\"status-badge " << badge_class << "\">"
          << stats->submodules.size() << " modules</span>\n";
        f << "          </div>\n";
        f << "        </div>\n";
        f << "        <div class=\"group-content\">\n";

        // Sort submodules by coverage
        std::vector<const ModuleCoverage*> sorted_subs = stats->submodules;
        std::sort(sorted_subs.begin(), sorted_subs.end(),
                  [](const ModuleCoverage* a, const ModuleCoverage* b) {
                      double pct_a = a->functions.empty()
                                         ? 0
                                         : (100.0 * a->covered_count / a->functions.size());
                      double pct_b = b->functions.empty()
                                         ? 0
                                         : (100.0 * b->covered_count / b->functions.size());
                      return pct_a < pct_b;
                  });

        for (const auto* sub : sorted_subs) {
            int sub_total = static_cast<int>(sub->functions.size());
            double sub_pct = sub_total > 0 ? (100.0 * sub->covered_count / sub_total) : 0;
            std::string sub_color = sub_pct < 10 ? "red" : (sub_pct < 50 ? "yellow" : "green");
            std::string sub_badge =
                sub_pct == 100.0 ? "badge-full" : (sub_pct == 0.0 ? "badge-none" : "badge-partial");
            std::string sub_badge_text =
                sub_pct == 100.0 ? "Full" : (sub_pct == 0.0 ? "None" : "Partial");

            // Show relative name (remove group prefix if present)
            std::string display_name = sub->name;
            if (display_name.find(name + "/") == 0) {
                display_name = display_name.substr(name.length() + 1);
            }

            f << "          <div class=\"submodule-row\" style=\"flex-direction: column; "
                 "align-items: stretch;\">\n";
            f << "            <div style=\"display: flex; justify-content: space-between; "
                 "align-items: center;\">\n";
            f << "              <span class=\"submodule-name\">" << display_name << "</span>\n";
            f << "              <div class=\"submodule-stats\">\n";
            f << "                <span style=\"color: var(--" << sub_color << ");\">" << std::fixed
              << std::setprecision(1) << sub_pct << "%</span>\n";
            f << "                <span>" << sub->covered_count << "/" << sub_total << "</span>\n";
            f << "                <span class=\"status-badge " << sub_badge << "\">"
              << sub_badge_text << "</span>\n";
            f << "              </div>\n";
            f << "            </div>\n";
            // Show all functions for this module
            f << "            <div class=\"func-list\" style=\"margin-top: 8px; padding-left: "
                 "16px; font-size: 12px;\">\n";
            for (const auto& func : sub->covered_functions) {
                f << "              <div style=\"color: var(--green);\">+ " << func << "</div>\n";
            }
            for (const auto& func : sub->uncovered_functions) {
                f << "              <div style=\"color: var(--red);\">✗ " << func << "</div>\n";
            }
            f << "            </div>\n";
            f << "          </div>\n";
        }

        f << "        </div>\n";
        f << "      </div>\n";
    }

    f << "      </div>\n"; // Close module-groups
    f << "    </div>\n";   // Close modules tab panel

    // Priority section - modules that need tests
    std::set<std::string> critical_modules = {
        "sync/mutex",    "sync/Arc",   "sync/rwlock", "sync/queue", "sync/stack",
        "hash",          "intrinsics", "num/integer", "ops/bit",    "fmt/impls",
        "fmt/formatter", "convert",    "error",       "json",       "pool"};

    struct PriorityModule {
        std::string name;
        int total;
        int covered;
        double pct;
        bool is_critical;
    };

    std::vector<PriorityModule> critical_list, zero_list, low_list;
    for (const auto& mod : modules) {
        if (mod.functions.empty())
            continue;
        int mod_total = static_cast<int>(mod.functions.size());
        double pct = 100.0 * mod.covered_count / mod_total;
        if (pct >= 50.0)
            continue;

        PriorityModule pm;
        pm.name = mod.name;
        pm.total = mod_total;
        pm.covered = mod.covered_count;
        pm.pct = pct;
        pm.is_critical = critical_modules.count(mod.name) > 0;

        if (pct == 0.0 && pm.is_critical) {
            critical_list.push_back(pm);
        } else if (pct == 0.0) {
            zero_list.push_back(pm);
        } else if (pct < 30.0) {
            low_list.push_back(pm);
        }
    }

    // Sort by uncovered count descending
    auto sort_by_uncovered = [](const PriorityModule& a, const PriorityModule& b) {
        return (a.total - a.covered) > (b.total - b.covered);
    };
    std::sort(critical_list.begin(), critical_list.end(), sort_by_uncovered);
    std::sort(zero_list.begin(), zero_list.end(), sort_by_uncovered);
    std::sort(low_list.begin(), low_list.end(), sort_by_uncovered);

    f << R"(
    <!-- Priorities Tab -->
    <div id="priorities" class="tab-panel">
      <h2 class="section-title">Test Improvement Priorities</h2>
      <div class="stats">
        <div class="stat-card">
          <div class="stat-value stat-red">)"
      << critical_list.size() << R"(</div>
          <div class="stat-label">Critical (0%, high priority)</div>
        </div>
        <div class="stat-card">
          <div class="stat-value stat-red">)"
      << zero_list.size() << R"(</div>
          <div class="stat-label">Zero Coverage (0%)</div>
        </div>
        <div class="stat-card">
          <div class="stat-value stat-yellow">)"
      << low_list.size() << R"(</div>
          <div class="stat-label">Low Coverage (<30%)</div>
        </div>
      </div>

      <table>
      <thead>
        <tr>
          <th>Priority</th>
          <th>Module</th>
          <th>Coverage</th>
          <th>Missing</th>
        </tr>
      </thead>
      <tbody>
)";

    // Critical modules
    for (size_t i = 0; i < critical_list.size() && i < 10; i++) {
        const auto& pm = critical_list[i];
        f << "        <tr>\n";
        f << "          <td><span class=\"status-badge badge-none\">CRITICAL</span></td>\n";
        f << "          <td class=\"module-name\">" << pm.name << "</td>\n";
        f << "          <td>" << pm.covered << " / " << pm.total << "</td>\n";
        f << "          <td style=\"color: var(--red);\">" << (pm.total - pm.covered)
          << " functions</td>\n";
        f << "        </tr>\n";
    }

    // Zero coverage (top 15)
    for (size_t i = 0; i < zero_list.size() && i < 15; i++) {
        const auto& pm = zero_list[i];
        f << "        <tr>\n";
        f << "          <td><span class=\"status-badge badge-none\">Zero</span></td>\n";
        f << "          <td class=\"module-name\">" << pm.name << "</td>\n";
        f << "          <td>" << pm.covered << " / " << pm.total << "</td>\n";
        f << "          <td style=\"color: var(--red);\">" << (pm.total - pm.covered)
          << " functions</td>\n";
        f << "        </tr>\n";
    }

    // Low coverage (top 15)
    for (size_t i = 0; i < low_list.size() && i < 15; i++) {
        const auto& pm = low_list[i];
        f << "        <tr>\n";
        f << "          <td><span class=\"status-badge badge-partial\">Low</span></td>\n";
        f << "          <td class=\"module-name\">" << pm.name << "</td>\n";
        f << "          <td>" << pm.covered << " / " << pm.total << " (" << std::fixed
          << std::setprecision(1) << pm.pct << "%)</td>\n";
        f << "          <td style=\"color: var(--yellow);\">" << (pm.total - pm.covered)
          << " functions</td>\n";
        f << "        </tr>\n";
    }

    f << R"(      </tbody>
      </table>
    </div>
)";

    // Uncovered functions section
    f << R"(
    <!-- Uncovered Tab -->
    <div id="uncovered" class="tab-panel">
      <h2 class="section-title">Uncovered Functions ()"
      << (total_funcs - total_covered) << R"( total)</h2>
)";

    if (!uncovered_by_module.empty()) {
        f << R"(      <div class="uncovered-section">
)";

        // Sort by number of uncovered (most uncovered first)
        std::sort(uncovered_by_module.begin(), uncovered_by_module.end(),
                  [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });

        for (const auto& [module_name, funcs] : uncovered_by_module) {
            f << "      <div class=\"uncovered-module\" "
                 "onclick=\"this.classList.toggle('expanded')\">\n";
            f << "        <div class=\"uncovered-header\">\n";
            f << "          <span class=\"module-name\">" << module_name << "</span>\n";
            f << "          <span class=\"uncovered-count\">" << funcs.size()
              << " uncovered</span>\n";
            f << "        </div>\n";
            f << "        <div class=\"uncovered-list\">\n";
            for (const auto& func : funcs) {
                f << "          <div class=\"uncovered-func\">" << func << "</div>\n";
            }
            f << "        </div>\n";
            f << "      </div>\n";
        }

        f << "      </div>\n"; // Close uncovered-section
    } else {
        f << "      <p style=\"color: var(--text-dim);\">No uncovered functions - excellent!</p>\n";
    }

    f << "    </div>\n"; // Close uncovered tab panel

    // Test Suites tab
    f << R"(
    <!-- Test Suites Tab -->
    <div id="suites" class="tab-panel">
      <h2 class="section-title">Test Suite Details</h2>
      <div class="suite-list">
)";

    // Write all test suites
    for (const auto& suite : test_stats.suites) {
        f << "        <div class=\"suite-item\">\n";
        f << "          <span class=\"suite-name\">" << suite.name << "</span>\n";
        f << "          <div class=\"suite-stats\">\n";
        f << "            <span class=\"suite-tests\">" << suite.test_count << " tests</span>\n";
        f << "            <span class=\"suite-duration\">" << suite.duration_ms << "ms</span>\n";
        f << "          </div>\n";
        f << "        </div>\n";
    }

    f << R"(      </div>

      <div class="stats" style="margin-top: 24px;">
        <div class="stat-card">
          <div class="stat-value stat-green">)"
      << tml_tests << R"(</div>
          <div class="stat-label">Total Tests</div>
        </div>
        <div class="stat-card">
          <div class="stat-value">)"
      << tml_files << R"(</div>
          <div class="stat-label">Test Files</div>
        </div>
        <div class="stat-card">
          <div class="stat-value">)"
      << tml_suites << R"(</div>
          <div class="stat-label">Test Suites</div>
        </div>
        <div class="stat-card">
          <div class="stat-value">)"
      << test_stats.total_duration_ms << R"(ms</div>
          <div class="stat-label">Total Duration</div>
        </div>
      </div>
    </div>

    <div class="footer">
      Generated by TML Compiler &bull; Click on module headers to expand details
    </div>
  </div>

  <script>
    function showTab(tabId) {
      // Hide all panels
      document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));

      // Show selected panel
      document.getElementById(tabId).classList.add('active');

      // Mark selected tab
      event.target.classList.add('active');
    }
  </script>
</body>
</html>
)";

    f.close();
    TML_LOG_INFO("test", "HTML report written to " << output_path);

    // Build set of all library functions for comparison
    std::set<std::string> library_functions;
    for (const auto& mod : modules) {
        for (const auto& func : mod.functions) {
            library_functions.insert(func);
        }
    }

    // Find functions that were called but not in library (generic instantiations, test funcs, etc.)
    std::vector<std::string> non_library_functions;
    for (const auto& func : covered_functions) {
        if (library_functions.find(func) == library_functions.end()) {
            non_library_functions.push_back(func);
        }
    }
    std::sort(non_library_functions.begin(), non_library_functions.end());

    // Generate JSON summary alongside the HTML report
    fs::path json_path = fs::path(output_path).replace_extension(".json");
    std::ofstream json_file(json_path);
    if (json_file.is_open()) {
        json_file << "{\n";
        json_file << "  \"library_functions\": " << total_funcs << ",\n";
        json_file << "  \"library_covered\": " << total_covered << ",\n";
        json_file << "  \"library_coverage_percent\": " << std::fixed << std::setprecision(2)
                  << overall_pct << ",\n";
        json_file << "  \"total_functions_called\": " << covered_functions.size() << ",\n";
        json_file << "  \"non_library_functions_called\": " << non_library_functions.size()
                  << ",\n";
        json_file << "  \"tests_passed\": " << tml_tests << ",\n";
        json_file << "  \"test_files\": " << tml_files << ",\n";
        json_file << "  \"test_suites\": " << tml_suites << ",\n";
        json_file << "  \"duration_ms\": " << test_stats.total_duration_ms << ",\n";
        json_file << "  \"modules_100_percent\": " << full_coverage << ",\n";
        json_file << "  \"modules_partial\": " << partial_coverage << ",\n";
        json_file << "  \"modules_zero_coverage\": " << zero_coverage << ",\n";
        json_file << "  \"suites\": [\n";
        for (size_t i = 0; i < test_stats.suites.size(); ++i) {
            const auto& suite = test_stats.suites[i];
            json_file << "    {\"name\": \"" << suite.name << "\", \"tests\": " << suite.test_count
                      << ", \"duration_ms\": " << suite.duration_ms << "}";
            if (i + 1 < test_stats.suites.size())
                json_file << ",";
            json_file << "\n";
        }
        json_file << "  ],\n";
        // Add non-library functions for debugging
        json_file << "  \"non_library_functions\": [\n";
        for (size_t i = 0; i < non_library_functions.size(); ++i) {
            json_file << "    \"" << non_library_functions[i] << "\"";
            if (i + 1 < non_library_functions.size())
                json_file << ",";
            json_file << "\n";
        }
        json_file << "  ]\n";
        json_file << "}\n";
        json_file.close();
    }
}

} // namespace tml::cli::tester
