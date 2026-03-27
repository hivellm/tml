TML_MODULE("test")

//! # Independent Coverage Reports
//!
//! Scans library source files, extracts function definitions,
//! compares against runtime coverage data, and generates reports.
//! Zero dependency on the old test system. Part of the v3 independent test system.

#include "testing/testing_coverage.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace tml::testing {

// ============================================================================
// ANSI color helpers
// ============================================================================

struct AnsiColor {
    bool enabled;
    const char* reset_v() const {
        return enabled ? "\033[0m" : "";
    }
    const char* bold_v() const {
        return enabled ? "\033[1m" : "";
    }
    const char* dim_v() const {
        return enabled ? "\033[2m" : "";
    }
    const char* red_v() const {
        return enabled ? "\033[31m" : "";
    }
    const char* green_v() const {
        return enabled ? "\033[32m" : "";
    }
    const char* yellow_v() const {
        return enabled ? "\033[33m" : "";
    }
    const char* cyan_v() const {
        return enabled ? "\033[36m" : "";
    }
};

// ============================================================================
// Module coverage data
// ============================================================================

struct ModuleCoverage {
    std::string name;
    std::vector<std::string> functions;
    std::vector<std::string> covered_functions;
    std::vector<std::string> uncovered_functions;
    int covered_count = 0;

    void deduplicate() {
        std::set<std::string> seen;
        std::vector<std::string> unique;
        for (const auto& f : functions) {
            if (seen.insert(f).second)
                unique.push_back(f);
        }
        functions = std::move(unique);
    }
};

// ============================================================================
// Hand-written keyword scanners (replaces std::regex for ~10x speedup)
// ============================================================================

/// Skip whitespace, return position of first non-whitespace char (or npos).
static size_t skip_ws(const std::string& s, size_t pos = 0) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'))
        ++pos;
    return pos < s.size() ? pos : std::string::npos;
}

/// Extract a \w+ identifier starting at pos. Returns empty string if not an identifier.
static std::string extract_ident(const std::string& s, size_t pos) {
    size_t start = pos;
    while (pos < s.size() && (std::isalnum(static_cast<unsigned char>(s[pos])) || s[pos] == '_'))
        ++pos;
    return pos > start ? s.substr(start, pos - start) : "";
}

/// Check if line matches: ^\s*(pub\s+)?keyword\s+(\w+)
/// Returns the captured identifier (group 2), or empty string if no match.
static std::string match_keyword_decl(const std::string& line, const char* keyword) {
    size_t pos = skip_ws(line);
    if (pos == std::string::npos)
        return "";

    // Optional "pub "
    if (line.compare(pos, 3, "pub") == 0 && pos + 3 < line.size() &&
        (line[pos + 3] == ' ' || line[pos + 3] == '\t')) {
        pos = skip_ws(line, pos + 3);
        if (pos == std::string::npos)
            return "";
    }

    size_t kw_len = std::strlen(keyword);
    if (line.compare(pos, kw_len, keyword) != 0)
        return "";
    pos += kw_len;
    if (pos >= line.size() || (line[pos] != ' ' && line[pos] != '\t'))
        return "";
    pos = skip_ws(line, pos);
    if (pos == std::string::npos)
        return "";
    return extract_ident(line, pos);
}

/// Skip optional generic bracket [...] at pos. Returns position after ] or pos if no bracket.
static size_t skip_brackets(const std::string& s, size_t pos) {
    if (pos < s.size() && s[pos] == '[') {
        int depth = 1;
        ++pos;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '[')
                ++depth;
            else if (s[pos] == ']')
                --depth;
            ++pos;
        }
    }
    return pos;
}

struct ImplParseResult {
    bool matched = false;
    bool is_tuple_target = false;
    std::string behavior_name; // first identifier (e.g., "From")
    std::string for_type;      // type after "for" (e.g., "I16"), empty if no "for"
};

/// Parse: ^\s*impl\s*[optional_generics]?\s*BehaviorName[optional_type_args]?\s*(for\s+Type)?
static ImplParseResult parse_impl_line(const std::string& line) {
    ImplParseResult result;
    size_t pos = skip_ws(line);
    if (pos == std::string::npos)
        return result;
    if (line.compare(pos, 4, "impl") != 0)
        return result;
    pos += 4;
    if (pos >= line.size() || (line[pos] != ' ' && line[pos] != '\t' && line[pos] != '['))
        return result;

    result.matched = true;

    // Skip optional generic params: impl[T]
    pos = skip_ws(line, pos);
    if (pos == std::string::npos)
        return result;
    pos = skip_brackets(line, pos);
    pos = skip_ws(line, pos);
    if (pos == std::string::npos)
        return result;

    // Extract behavior name
    result.behavior_name = extract_ident(line, pos);
    pos += result.behavior_name.size();

    // Skip optional type args: BehaviorName[I8]
    pos = skip_brackets(line, pos);

    // Check for "for"
    pos = skip_ws(line, pos);
    if (pos != std::string::npos && line.compare(pos, 3, "for") == 0 && pos + 3 < line.size() &&
        (line[pos + 3] == ' ' || line[pos + 3] == '\t')) {
        pos = skip_ws(line, pos + 3);
        if (pos != std::string::npos) {
            if (line[pos] == '(') {
                result.is_tuple_target = true;
            } else {
                result.for_type = extract_ident(line, pos);
            }
        }
    }

    return result;
}

// ============================================================================
// Function extractor
// ============================================================================

/// Extract function names from a TML source file.
/// Returns names like "func_name" or "TypeName::method_name".
static std::vector<std::string> extract_functions(const fs::path& file) {
    std::vector<std::string> functions;
    std::string current_impl;
    bool in_behavior = false;
    bool in_class = false;     // class methods have no coverage instrumentation
    bool in_interface = false; // interface methods have no coverage instrumentation
    int impl_brace_depth = 0;

    std::ifstream ifs(file);
    if (!ifs.is_open())
        return functions;

    std::string line;
    std::string prev_line;

    while (std::getline(ifs, line)) {
        // Check for class/interface/behavior/impl declarations
        std::string name;
        if (!(name = match_keyword_decl(line, "class")).empty()) {
            current_impl = name;
            in_behavior = false;
            in_class = true;
            in_interface = false;
            impl_brace_depth = 0;
        } else if (!(name = match_keyword_decl(line, "interface")).empty()) {
            current_impl = name;
            in_behavior = true;
            in_class = false;
            in_interface = true;
            impl_brace_depth = 0;
        } else {
            auto impl = parse_impl_line(line);
            if (impl.matched && impl.is_tuple_target) {
                // impl ... for (A, B) — tuple types emit under concrete tuple names
                // at runtime (Tuple2, etc.) which aren't directly matchable from source.
                current_impl = "__generic_impl__";
                in_behavior = false;
                in_class = false;
                in_interface = false;
                impl_brace_depth = 0;
            } else if (impl.matched) {
                if (!impl.for_type.empty()) {
                    if (impl.for_type.size() == 1 && std::isupper(impl.for_type[0])) {
                        // impl Behavior[T] for T — generic type param as impl target.
                        current_impl = "__generic_impl__";
                    } else {
                        current_impl = impl.for_type;
                    }
                } else {
                    current_impl = impl.behavior_name;
                }
                in_behavior = false;
                in_class = false;
                in_interface = false;
                impl_brace_depth = 0;
            } else if (!(name = match_keyword_decl(line, "behavior")).empty()) {
                current_impl = name;
                in_behavior = true;
                in_class = false;
                in_interface = false;
                impl_brace_depth = 0;
            }
        }

        // Skip brace counting for comment/docstring lines — unbalanced braces
        // in doc examples (e.g., `/// let err = f("expected '}'", 42)`) would
        // otherwise decrement depth prematurely and drop the current impl context.
        bool is_comment_line = false;
        {
            size_t non_space = line.find_first_not_of(" \t");
            if (non_space != std::string::npos && line[non_space] == '/' &&
                non_space + 1 < line.size() && line[non_space + 1] == '/') {
                is_comment_line = true;
            }
        }
        if (!current_impl.empty() && !is_comment_line) {
            for (char c : line) {
                if (c == '{')
                    impl_brace_depth++;
                else if (c == '}')
                    impl_brace_depth--;
            }
            if (impl_brace_depth <= 0) {
                current_impl.clear();
                in_behavior = false;
                in_class = false;
                in_interface = false;
                impl_brace_depth = 0;
            }
        }

        std::string func_name = match_keyword_decl(line, "func");
        if (!func_name.empty()) {
            if (func_name.rfind("test_", 0) == 0) {
                prev_line = line;
                continue;
            }
            if (prev_line.find("@extern(") != std::string::npos) {
                prev_line = line;
                continue;
            }
            // Skip functions annotated with @no_coverage (genuinely untestable)
            if (prev_line.find("@no_coverage") != std::string::npos) {
                prev_line = line;
                continue;
            }
            if (func_name.rfind("ffi_", 0) == 0) {
                prev_line = line;
                continue;
            }
            // Skip behavior methods entirely — declarations have no body,
            // defaults (generate_default_method) have no emit_coverage() call
            if (in_behavior) {
                prev_line = line;
                continue;
            }
            // Skip class/interface methods — no coverage instrumentation in codegen
            if (in_class || in_interface) {
                prev_line = line;
                continue;
            }
            // Skip Drop::drop — auto-called by runtime, never instrumented
            if (func_name == "drop") {
                prev_line = line;
                continue;
            }

            // Skip methods in generic impl-for-TypeParam blocks — they emit under
            // concrete type names at runtime (e.g. I32::from not From::from)
            if (current_impl == "__generic_impl__") {
                prev_line = line;
                continue;
            }

            if (!current_impl.empty()) {
                functions.push_back(current_impl + "::" + func_name);
            } else {
                functions.push_back(func_name);
            }
        }
        prev_line = line;
    }

    // Deduplicate: multiple impls can emit the same qualified name
    // (e.g., impl From[I8] for I32 and impl From[I16] for I32 both emit I32::from).
    // Coverage runtime only tracks one hit per name, so duplicates cause false negatives.
    {
        std::vector<std::string> unique;
        std::set<std::string> seen;
        for (const auto& f : functions) {
            if (seen.insert(f).second)
                unique.push_back(f);
        }
        functions = std::move(unique);
    }

    return functions;
}

/// Get module name from file path relative to a library base.
static std::string get_module_name(const fs::path& file, const fs::path& base) {
    auto rel = fs::relative(file, base);
    std::string result = rel.string();
    std::replace(result.begin(), result.end(), '\\', '/');

    size_t src_pos = result.find("/src/");
    if (src_pos != std::string::npos)
        result = result.substr(src_pos + 5);
    else if (result.rfind("src/", 0) == 0)
        result = result.substr(4);

    if (result.size() > 4 && result.substr(result.size() - 4) == ".tml")
        result = result.substr(0, result.size() - 4);
    if (result.size() > 4 && result.substr(result.size() - 4) == "/mod")
        result = result.substr(0, result.size() - 4);

    return result;
}

/// Scan library directories for all function definitions.
static std::vector<ModuleCoverage> scan_library(const std::vector<fs::path>& lib_dirs) {
    std::unordered_map<std::string, std::vector<std::string>> modules;

    for (const auto& lib_dir : lib_dirs) {
        if (!fs::exists(lib_dir))
            continue;
        for (const auto& entry : fs::recursive_directory_iterator(lib_dir)) {
            if (!entry.is_regular_file())
                continue;
            auto filename = entry.path().filename().string();
            if (filename.find(".test.tml") != std::string::npos)
                continue;
            if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".tml")
                continue;

            std::string path_str = entry.path().string();
            std::replace(path_str.begin(), path_str.end(), '\\', '/');
            if (path_str.find("/tests/") != std::string::npos)
                continue;

            auto funcs = extract_functions(entry.path());
            if (!funcs.empty()) {
                auto module = get_module_name(entry.path(), lib_dir);
                auto& mod_funcs = modules[module];
                mod_funcs.insert(mod_funcs.end(), funcs.begin(), funcs.end());
            }
        }
    }

    std::vector<ModuleCoverage> result;
    for (auto& [name, funcs] : modules) {
        ModuleCoverage mc;
        mc.name = name;
        mc.functions = std::move(funcs);
        mc.deduplicate();
        result.push_back(std::move(mc));
    }
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });
    return result;
}

/// Add builtin methods and compute coverage for all modules.
static void compute_coverage(std::vector<ModuleCoverage>& modules,
                             const std::set<std::string>& covered_functions, int& total_funcs,
                             int& total_covered) {
    // Add builtin methods
    ModuleCoverage builtins;
    builtins.name = "builtins";
    builtins.functions = {"Slice::len",         "Slice::is_empty",  "MutSlice::len",
                          "MutSlice::is_empty", "Array::len",       "Array::is_empty",
                          "Array::get",         "Array::first",     "Array::last",
                          "Array::map",         "Array::eq",        "Array::ne",
                          "Array::cmp",         "Maybe::is_just",   "Maybe::is_nothing",
                          "Maybe::unwrap",      "Maybe::unwrap_or", "Maybe::map"};
    modules.insert(modules.begin(), std::move(builtins));

    total_funcs = 0;
    total_covered = 0;

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
    }
}

// ============================================================================
// Console report
// ============================================================================

void print_coverage_report(const std::set<std::string>& covered_functions, bool use_color,
                           const CoverageStats& stats) {
    (void)stats;

    fs::path cwd = fs::current_path();
    std::vector<fs::path> lib_dirs = {cwd / "lib" / "core", cwd / "lib" / "std",
                                      cwd / "lib" / "test"};

    auto modules = scan_library(lib_dirs);
    if (modules.empty())
        return;

    int total_funcs = 0, total_covered = 0;
    compute_coverage(modules, covered_functions, total_funcs, total_covered);

    AnsiColor c{use_color};
    std::vector<std::pair<std::string, std::vector<std::string>>> uncovered_by_module;

    for (const auto& mod : modules) {
        if (!mod.uncovered_functions.empty())
            uncovered_by_module.push_back({mod.name, mod.uncovered_functions});
    }

    // Header
    TML_LOG_INFO(
        "test",
        c.cyan_v()
            << c.bold_v()
            << "================================================================================"
            << c.reset_v());
    TML_LOG_INFO("test", c.cyan_v() << c.bold_v() << "                    LIBRARY COVERAGE ANALYSIS"
                                    << c.reset_v());
    TML_LOG_INFO(
        "test",
        c.cyan_v()
            << c.bold_v()
            << "================================================================================"
            << c.reset_v());

    // Overall summary
    double overall_pct = total_funcs > 0 ? (100.0 * total_covered / total_funcs) : 0.0;
    {
        std::ostringstream oss;
        oss << " Library Coverage: " << c.bold_v() << total_covered << "/" << total_funcs
            << c.reset_v() << " functions (" << c.bold_v();
        if (overall_pct < 10)
            oss << c.red_v();
        else if (overall_pct < 50)
            oss << c.yellow_v();
        else
            oss << c.green_v();
        oss << std::fixed << std::setprecision(1) << overall_pct << "%" << c.reset_v() << ")";
        TML_LOG_INFO("test", oss.str());
    }
    TML_LOG_INFO("test", " Total Functions Called: " << c.green_v() << c.bold_v()
                                                     << covered_functions.size() << c.reset_v());

    // Per-module table
    TML_LOG_INFO(
        "test",
        c.dim_v()
            << "--------------------------------------------------------------------------------"
            << c.reset_v());
    {
        std::ostringstream hdr;
        hdr << " " << std::left << std::setw(45) << "Module" << std::right << std::setw(12)
            << "Coverage" << std::setw(10) << "Percent";
        TML_LOG_INFO("test", hdr.str());
    }
    TML_LOG_INFO(
        "test",
        c.dim_v()
            << "--------------------------------------------------------------------------------"
            << c.reset_v());

    for (const auto& mod : modules) {
        int mod_total = static_cast<int>(mod.functions.size());
        double pct = mod_total > 0 ? (100.0 * mod.covered_count / mod_total) : 0.0;
        const char* status = pct == 100.0 ? "+" : (pct == 0.0 ? "X" : "~");
        const char* color = pct == 100.0 ? c.green_v() : (pct == 0.0 ? c.red_v() : c.yellow_v());

        {
            std::ostringstream row;
            row << " " << color << status << c.reset_v() << " " << std::left << std::setw(43)
                << mod.name << std::right << std::setw(5) << mod.covered_count << "/" << std::left
                << std::setw(5) << mod_total << std::right << color << std::setw(9) << std::fixed
                << std::setprecision(1) << pct << "%" << c.reset_v();
            TML_LOG_INFO("test", row.str());
        }

        for (const auto& func : mod.covered_functions) {
            TML_LOG_INFO("test", "      " << c.green_v() << "+" << c.reset_v() << " " << c.dim_v()
                                          << func << c.reset_v());
        }
        for (const auto& func : mod.uncovered_functions) {
            TML_LOG_INFO("test", "      " << c.red_v() << "X" << c.reset_v() << " " << c.dim_v()
                                          << func << c.reset_v());
        }
    }

    // Summary line
    int zero_coverage_modules = 0;
    for (const auto& mod : modules) {
        if (mod.covered_count == 0 && !mod.functions.empty())
            zero_coverage_modules++;
    }

    TML_LOG_INFO(
        "test",
        c.dim_v()
            << "--------------------------------------------------------------------------------"
            << c.reset_v());
    TML_LOG_INFO("test", " " << c.red_v() << c.bold_v() << zero_coverage_modules << c.reset_v()
                             << " modules with 0% coverage");
    TML_LOG_INFO(
        "test",
        c.dim_v()
            << "================================================================================"
            << c.reset_v());

    // Priority table
    TML_LOG_INFO(
        "test",
        c.cyan_v()
            << c.bold_v()
            << "================================================================================"
            << c.reset_v());
    TML_LOG_INFO("test", c.cyan_v()
                             << c.bold_v() << "                    TEST IMPROVEMENT PRIORITIES"
                             << c.reset_v());
    TML_LOG_INFO(
        "test",
        c.cyan_v()
            << c.bold_v()
            << "================================================================================"
            << c.reset_v());

    struct PriorityModule {
        std::string name;
        int total;
        int covered;
        double pct;
        bool is_critical;
    };

    std::set<std::string> critical_modules = {
        "sync/mutex",    "sync/Arc",   "sync/rwlock", "sync/queue", "sync/stack",
        "hash",          "intrinsics", "num/integer", "ops/bit",    "fmt/impls",
        "fmt/formatter", "convert",    "error",       "json",       "pool"};

    std::vector<PriorityModule> priority_list;
    for (const auto& mod : modules) {
        if (mod.functions.empty())
            continue;
        int mod_total = static_cast<int>(mod.functions.size());
        double pct = 100.0 * mod.covered_count / mod_total;
        if (pct < 50.0) {
            priority_list.push_back({mod.name, mod_total, mod.covered_count, pct,
                                     critical_modules.count(mod.name) > 0});
        }
    }
    std::sort(priority_list.begin(), priority_list.end(),
              [](const PriorityModule& a, const PriorityModule& b) {
                  if (a.is_critical != b.is_critical)
                      return a.is_critical > b.is_critical;
                  return (a.total - a.covered) > (b.total - b.covered);
              });

    // Critical
    TML_LOG_INFO("test",
                 " " << c.red_v() << c.bold_v() << "CRITICAL (0% - high priority):" << c.reset_v());
    TML_LOG_INFO(
        "test",
        c.dim_v() << " -----------------------------------------------------------------------"
                  << c.reset_v());
    int critical_count = 0;
    for (const auto& pm : priority_list) {
        if (pm.pct == 0.0 && pm.is_critical && critical_count < 10) {
            std::ostringstream row;
            row << "  " << c.red_v() << "•" << c.reset_v() << " " << std::left << std::setw(35)
                << pm.name << std::right << std::setw(4) << pm.covered << "/" << std::left
                << std::setw(4) << pm.total << " " << c.red_v() << "HIGH PRIORITY" << c.reset_v();
            TML_LOG_INFO("test", row.str());
            critical_count++;
        }
    }
    if (critical_count == 0)
        TML_LOG_INFO("test", "  " << c.dim_v() << "(none)" << c.reset_v());

    // Zero coverage
    TML_LOG_INFO("test", " " << c.red_v() << c.bold_v()
                             << "ZERO COVERAGE (0% - most functions):" << c.reset_v());
    TML_LOG_INFO(
        "test",
        c.dim_v() << " -----------------------------------------------------------------------"
                  << c.reset_v());
    int zero_count = 0;
    for (const auto& pm : priority_list) {
        if (pm.pct == 0.0 && !pm.is_critical && zero_count < 15) {
            std::ostringstream row;
            row << "  " << c.red_v() << "•" << c.reset_v() << " " << std::left << std::setw(35)
                << pm.name << std::right << std::setw(4) << pm.covered << "/" << std::left
                << std::setw(4) << pm.total << " (" << (pm.total - pm.covered) << " missing)";
            TML_LOG_INFO("test", row.str());
            zero_count++;
        }
    }

    // Low coverage
    TML_LOG_INFO("test",
                 " " << c.yellow_v() << c.bold_v() << "LOW COVERAGE (<30%):" << c.reset_v());
    TML_LOG_INFO(
        "test",
        c.dim_v() << " -----------------------------------------------------------------------"
                  << c.reset_v());
    int low_count = 0;
    for (const auto& pm : priority_list) {
        if (pm.pct > 0.0 && pm.pct < 30.0 && low_count < 15) {
            std::ostringstream row;
            row << "  " << c.yellow_v() << "~" << c.reset_v() << " " << std::left << std::setw(35)
                << pm.name << std::right << std::setw(4) << pm.covered << "/" << std::left
                << std::setw(4) << pm.total << " " << c.yellow_v() << std::fixed
                << std::setprecision(1) << pm.pct << "%" << c.reset_v();
            TML_LOG_INFO("test", row.str());
            low_count++;
        }
    }

    TML_LOG_INFO(
        "test",
        c.dim_v()
            << "================================================================================"
            << c.reset_v());

    // Uncovered functions by module
    if (!uncovered_by_module.empty()) {
        TML_LOG_INFO("test", c.cyan_v() << c.bold_v()
                                        << "======================================================="
                                           "========================="
                                        << c.reset_v());
        TML_LOG_INFO("test", c.cyan_v() << c.bold_v()
                                        << "                    UNCOVERED FUNCTIONS BY MODULE"
                                        << c.reset_v());
        TML_LOG_INFO("test", c.cyan_v() << c.bold_v()
                                        << "======================================================="
                                           "========================="
                                        << c.reset_v());

        auto sorted = uncovered_by_module;
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });

        int shown_modules = 0;
        for (const auto& [module_name, funcs] : sorted) {
            if (shown_modules >= 20)
                break;
            shown_modules++;

            TML_LOG_INFO("test", " " << c.yellow_v() << c.bold_v() << module_name << c.reset_v()
                                     << " " << c.dim_v() << "(" << funcs.size() << " uncovered)"
                                     << c.reset_v());

            int shown_funcs = 0;
            for (const auto& func : funcs) {
                if (shown_funcs >= 10) {
                    TML_LOG_INFO("test", "   " << c.dim_v() << "... and " << (funcs.size() - 10)
                                               << " more" << c.reset_v());
                    break;
                }
                TML_LOG_INFO("test", "   " << c.red_v() << "X" << c.reset_v() << " " << c.dim_v()
                                           << func << c.reset_v());
                shown_funcs++;
            }
        }

        if (sorted.size() > 20) {
            TML_LOG_INFO("test", " " << c.dim_v() << "... and " << (sorted.size() - 20)
                                     << " more modules with uncovered functions" << c.reset_v());
        }

        TML_LOG_INFO("test", c.dim_v() << "========================================================"
                                          "========================"
                                       << c.reset_v());
    }
}

// ============================================================================
// Previous coverage reader (for regression detection)
// ============================================================================

PreviousCoverage get_previous_coverage_from_json(const std::string& html_path) {
    PreviousCoverage result;

    // JSON file lives alongside HTML with .json extension
    fs::path json_path = fs::path(html_path).replace_extension(".json");

    if (!fs::exists(json_path))
        return result;

    std::ifstream file(json_path);
    if (!file.is_open())
        return result;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Parse total_functions and covered_functions from JSON
    // Format: "total_functions": N, "covered_functions": M, "coverage_percent": X.X
    auto extract_int = [&](const std::string& key) -> int {
        std::string needle = "\"" + key + "\"";
        auto pos = content.find(needle);
        if (pos == std::string::npos)
            return 0;
        pos += needle.size();
        // Skip whitespace and colon
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t' ||
                                        content[pos] == ':' || content[pos] == '\n'))
            ++pos;
        // Extract digits
        size_t start = pos;
        while (pos < content.size() && std::isdigit(static_cast<unsigned char>(content[pos])))
            ++pos;
        if (pos > start) {
            try {
                return std::stoi(content.substr(start, pos - start));
            } catch (...) {}
        }
        return 0;
    };

    result.total = extract_int("total_functions");
    result.covered = extract_int("covered_functions");

    if (result.total > 0) {
        result.percent = (100.0 * result.covered) / result.total;
        result.valid = true;
    }

    return result;
}

} // namespace tml::testing
