//! # Library Coverage Analysis
//!
//! Scans library source files to find all function definitions,
//! then compares against runtime coverage data to report what's NOT covered.

#include "log/log.hpp"
#include "tester_internal.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
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
    bool in_behavior = false; // True when inside a behavior block (vs impl block)
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
            in_behavior = false;
            impl_brace_depth = 0; // Reset depth, will count opening brace below
        }
        // Track behavior blocks - detect "behavior BehaviorName" or "pub behavior BehaviorName"
        else if (std::regex_search(line, match, behavior_regex)) {
            current_impl = match[2].str();
            in_behavior = true;
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
                in_behavior = false;
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

            // Skip bodyless declarations inside behavior blocks.
            // Behavior blocks define interfaces; only methods with bodies (default impls)
            // should be counted. A declaration without '{' on its line is bodyless.
            // The actual implementations live in `impl Behavior for Type` blocks.
            if (in_behavior && line.find('{') == std::string::npos) {
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
    TML_LOG_INFO(
        "test",
        c.cyan()
            << c.bold()
            << "================================================================================"
            << c.reset());
    TML_LOG_INFO("test", c.cyan() << c.bold() << "                    LIBRARY COVERAGE ANALYSIS"
                                  << c.reset());
    TML_LOG_INFO(
        "test",
        c.cyan()
            << c.bold()
            << "================================================================================"
            << c.reset());

    // Overall summary
    double overall_pct = total_funcs > 0 ? (100.0 * total_covered / total_funcs) : 0.0;
    {
        std::ostringstream oss;
        oss << " Library Coverage: " << c.bold() << total_covered << "/" << total_funcs << c.reset()
            << " functions (" << c.bold();
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
    TML_LOG_INFO(
        "test",
        c.dim()
            << "--------------------------------------------------------------------------------"
            << c.reset());
    {
        std::ostringstream hdr;
        hdr << " " << std::left << std::setw(45) << "Module" << std::right << std::setw(12)
            << "Coverage" << std::setw(10) << "Percent";
        TML_LOG_INFO("test", hdr.str());
    }
    TML_LOG_INFO(
        "test",
        c.dim()
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
                    << mod.name << std::right << std::setw(5) << mod.covered_count << "/"
                    << std::left << std::setw(5) << mod_total << std::right << color << std::setw(9)
                    << std::fixed << std::setprecision(1) << pct << "%" << c.reset();
            TML_LOG_INFO("test", mod_row.str());
        }

        // Always show function details for every module
        for (const auto& func : mod.covered_functions) {
            TML_LOG_INFO("test", "      " << c.green() << "+" << c.reset() << " " << c.dim() << func
                                          << c.reset());
        }
        for (const auto& func : mod.uncovered_functions) {
            TML_LOG_INFO("test", "      " << c.red() << "X" << c.reset() << " " << c.dim() << func
                                          << c.reset());
        }
    }

    // Count modules with 0% coverage
    int zero_coverage_modules = 0;
    for (const auto& mod : modules) {
        if (mod.covered_count == 0 && !mod.functions.empty()) {
            zero_coverage_modules++;
        }
    }

    TML_LOG_INFO(
        "test",
        c.dim()
            << "--------------------------------------------------------------------------------"
            << c.reset());
    TML_LOG_INFO("test", " " << c.red() << c.bold() << zero_coverage_modules << c.reset()
                             << " modules with 0% coverage");
    TML_LOG_INFO(
        "test",
        c.dim()
            << "================================================================================"
            << c.reset());

    // Priority table - modules that need tests
    TML_LOG_INFO(
        "test",
        c.cyan()
            << c.bold()
            << "================================================================================"
            << c.reset());
    TML_LOG_INFO("test", c.cyan() << c.bold() << "                    TEST IMPROVEMENT PRIORITIES"
                                  << c.reset());
    TML_LOG_INFO(
        "test",
        c.cyan()
            << c.bold()
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
    TML_LOG_INFO("test",
                 " " << c.red() << c.bold() << "CRITICAL (0% - high priority):" << c.reset());
    TML_LOG_INFO(
        "test",
        c.dim() << " -----------------------------------------------------------------------"
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
    TML_LOG_INFO(
        "test",
        c.dim() << " -----------------------------------------------------------------------"
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
    TML_LOG_INFO("test", " " << c.yellow() << c.bold() << "LOW COVERAGE (<30%):" << c.reset());
    TML_LOG_INFO(
        "test",
        c.dim() << " -----------------------------------------------------------------------"
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

    TML_LOG_INFO(
        "test",
        c.dim()
            << "================================================================================"
            << c.reset());

    // Print uncovered functions by module
    if (!uncovered_by_module.empty()) {
        TML_LOG_INFO("test", c.cyan() << c.bold()
                                      << "========================================================="
                                         "======================="
                                      << c.reset());
        TML_LOG_INFO("test", c.cyan()
                                 << c.bold() << "                    UNCOVERED FUNCTIONS BY MODULE"
                                 << c.reset());
        TML_LOG_INFO("test", c.cyan() << c.bold()
                                      << "========================================================="
                                         "======================="
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
                                     << c.dim() << "(" << funcs.size() << " uncovered)"
                                     << c.reset());

            // Show up to 10 functions per module
            int shown_funcs = 0;
            for (const auto& func : funcs) {
                if (shown_funcs >= 10) {
                    TML_LOG_INFO("test", "   " << c.dim() << "... and " << (funcs.size() - 10)
                                               << " more" << c.reset());
                    break;
                }
                TML_LOG_INFO("test", "   " << c.red() << "✗" << c.reset() << " " << c.dim() << func
                                           << c.reset());
                shown_funcs++;
            }
        }

        if (sorted_uncovered.size() > 20) {
            TML_LOG_INFO("test", " " << c.dim() << "... and " << (sorted_uncovered.size() - 20)
                                     << " more modules with uncovered functions" << c.reset());
        }

        TML_LOG_INFO("test", c.dim() << "=========================================================="
                                        "======================"
                                     << c.reset());
    }
}

} // namespace tml::cli::tester
