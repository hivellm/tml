TML_MODULE("test")

//! # Independent Coverage Reports
//!
//! Scans library source files, extracts function definitions,
//! compares against runtime coverage data, and generates reports.
//! Zero dependency on the old test system. Part of the v3 independent test system.

#include "log/log.hpp"
#include "testing/testing_coverage.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace tml::testing {

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

            // Exclude platform-specific modules that can't be tested on this architecture
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
            // Skip ARM NEON stubs on x86 — they're compile-time stubs, not real functions
            if (path_str.find("/neon") != std::string::npos)
                continue;
#elif defined(__aarch64__) || defined(_M_ARM64)
            // Skip x86 SSE/AVX intrinsics on ARM
            if (path_str.find("/sse42") != std::string::npos ||
                path_str.find("/avx") != std::string::npos)
                continue;
#endif

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

// ============================================================================
// HTML + JSON report
// ============================================================================

void write_coverage_html(const std::set<std::string>& covered_functions,
                         const std::string& output_path, const CoverageStats& stats) {
    fs::path cwd = fs::current_path();
    std::vector<fs::path> lib_dirs = {cwd / "lib" / "core", cwd / "lib" / "std",
                                      cwd / "lib" / "test"};

    auto modules = scan_library(lib_dirs);
    if (modules.empty())
        return;

    int total_funcs = 0, total_covered = 0;
    compute_coverage(modules, covered_functions, total_funcs, total_covered);

    double overall_pct = total_funcs > 0 ? (100.0 * total_covered / total_funcs) : 0.0;

    // Calculate suite totals
    int tml_tests = 0, tml_suites = 0;
    for (const auto& suite : stats.suites) {
        tml_tests += suite.test_count;
        tml_suites++;
    }

    int full_coverage = 0, partial_coverage = 0, zero_coverage = 0;
    std::vector<std::pair<std::string, std::vector<std::string>>> uncovered_by_module;
    for (const auto& mod : modules) {
        if (mod.functions.empty())
            continue;
        if (mod.covered_count == 0)
            zero_coverage++;
        else if (mod.covered_count == static_cast<int>(mod.functions.size()))
            full_coverage++;
        else
            partial_coverage++;
        if (!mod.uncovered_functions.empty())
            uncovered_by_module.push_back({mod.name, mod.uncovered_functions});
    }

    // Write HTML
    std::ofstream f(output_path);
    if (!f.is_open()) {
        TML_LOG_ERROR("test", "Cannot write coverage HTML to " << output_path);
        return;
    }

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
    .stats-main { grid-template-columns: repeat(5, 1fr); }
    .stats-secondary { grid-template-columns: repeat(3, 1fr); max-width: 600px; }
    @media (max-width: 900px) { .stats-main { grid-template-columns: repeat(3, 1fr); } }
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
    .progress-fill { height: 100%; transition: width 0.3s; }
    .progress-text { text-align: center; margin-top: 8px; color: var(--text-dim); font-size: 14px; }

    /* Section */
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
    th, td { padding: 12px 16px; text-align: left; border-bottom: 1px solid var(--border); }
    th { background: var(--bg); font-weight: 600; color: var(--text-dim); font-size: 12px; text-transform: uppercase; }
    tr:last-child td { border-bottom: none; }
    tr:hover { background: rgba(88, 166, 255, 0.05); }
    .module-name { font-family: monospace; }
    .coverage-green { background: var(--green); }
    .coverage-yellow { background: var(--yellow); }
    .coverage-red { background: var(--red); }
    .status-badge { display: inline-block; padding: 2px 8px; border-radius: 12px; font-size: 12px; font-weight: 500; }
    .badge-full { background: rgba(63, 185, 80, 0.2); color: var(--green); }
    .badge-partial { background: rgba(210, 153, 34, 0.2); color: var(--yellow); }
    .badge-none { background: rgba(248, 81, 73, 0.2); color: var(--red); }

    /* Module groups (accordion) */
    .module-groups { display: flex; flex-direction: column; gap: 8px; }
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
    .group-title::before { content: "\25B6"; font-size: 10px; transition: transform 0.2s; }
    .module-group.expanded .group-title::before { transform: rotate(90deg); }
    .group-stats { display: flex; align-items: center; gap: 16px; font-size: 13px; }
    .group-coverage { display: flex; align-items: center; gap: 8px; }
    .group-bar { width: 80px; height: 6px; background: var(--border); border-radius: 3px; overflow: hidden; }
    .group-bar-fill { height: 100%; }
    .group-content { display: none; border-top: 1px solid var(--border); }
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
    .submodule-name { font-family: monospace; color: var(--text-dim); }
    .submodule-stats { display: flex; align-items: center; gap: 12px; }

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
    .tab:hover { color: var(--text); background: rgba(88, 166, 255, 0.05); }
    .tab.active { color: var(--blue); border-bottom-color: var(--blue); }
    .tab-panel { display: none; }
    .tab-panel.active { display: block; }

    /* Search */
    .search-box { margin-bottom: 16px; position: relative; }
    .search-input {
      width: 100%;
      padding: 10px 16px 10px 40px;
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text);
      font-size: 14px;
      font-family: monospace;
      outline: none;
      box-sizing: border-box;
    }
    .search-input:focus { border-color: var(--blue); box-shadow: 0 0 0 2px rgba(88, 166, 255, 0.2); }
    .search-input::placeholder { color: var(--text-dim); }
    .search-icon {
      position: absolute;
      left: 14px;
      top: 50%;
      transform: translateY(-50%);
      color: var(--text-dim);
      font-size: 14px;
      pointer-events: none;
    }
    .search-count {
      position: absolute;
      right: 14px;
      top: 50%;
      transform: translateY(-50%);
      color: var(--text-dim);
      font-size: 12px;
    }

    /* Uncovered */
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
    .uncovered-count { color: var(--red); font-size: 12px; }
    .uncovered-list { padding: 12px 16px; display: none; }
    .uncovered-module.expanded .uncovered-list { display: block; }
    .uncovered-func { font-family: monospace; font-size: 13px; padding: 4px 0; color: var(--text-dim); }
    .uncovered-func::before { content: "\2717 "; color: var(--red); }

    /* Test suites */
    .suite-list { display: flex; flex-direction: column; gap: 8px; }
    .suite-item {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .suite-name { font-weight: 600; font-family: monospace; }
    .suite-stats { display: flex; gap: 16px; align-items: center; color: var(--text-dim); font-size: 13px; }
    .suite-tests { color: var(--green); font-weight: 500; }
    .suite-duration { color: var(--text-dim); }

    /* Footer */
    .footer {
      margin-top: 32px;
      padding-top: 16px;
      border-top: 1px solid var(--border);
      color: var(--text-dim);
      font-size: 12px;
      text-align: center;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>TML Library Coverage Report</h1>
    <p class="subtitle">Generated by TML Test Runner (v3)</p>

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
        <div class="stat-label">Runtime Functions Called</div>
      </div>
      <div class="stat-card">
        <div class="stat-value stat-green">)"
      << tml_tests << R"(</div>
        <div class="stat-label">Tests Passed</div>
      </div>
      <div class="stat-card">
        <div class="stat-value">)"
      << stats.total_files << R"(</div>
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
    for (const auto& suite : stats.suites) {
        f << "        <div class=\"suite-item\">\n";
        f << "          <span class=\"suite-name\">" << suite.name << "</span>\n";
        f << "          <div class=\"suite-stats\">\n";
        f << "            <span class=\"suite-tests\">" << suite.test_count << " tests</span>\n";
        f << "            <span class=\"suite-duration\">" << suite.duration_ms << "ms</span>\n";
        f << "          </div>\n";
        f << "        </div>\n";
    }

    f << R"HTML(      </div>
    </div>

    <!-- Modules Tab -->
    <div id="modules" class="tab-panel">
      <h2 class="section-title">Module Coverage</h2>
      <div class="search-box">
        <span class="search-icon">&#128269;</span>
        <input type="text" class="search-input" id="moduleSearch" placeholder="Filter modules and functions..." oninput="filterModules(this.value)">
        <span class="search-count" id="moduleSearchCount"></span>
      </div>
      <div class="module-groups">
)HTML";

    // Group modules by top-level category
    struct GroupStats {
        std::string name;
        int total_funcs_g = 0;
        int covered_funcs_g = 0;
        std::vector<const ModuleCoverage*> submodules;
    };
    std::map<std::string, GroupStats> groups;

    for (const auto& mod : modules) {
        std::string group_name = mod.name;
        size_t slash_pos = mod.name.find('/');
        if (slash_pos != std::string::npos)
            group_name = mod.name.substr(0, slash_pos);
        auto& group = groups[group_name];
        group.name = group_name;
        group.total_funcs_g += static_cast<int>(mod.functions.size());
        group.covered_funcs_g += mod.covered_count;
        group.submodules.push_back(&mod);
    }

    // Sort groups by coverage percentage (lowest first)
    std::vector<std::pair<std::string, GroupStats*>> sorted_groups;
    for (auto& [name, gs] : groups)
        sorted_groups.push_back({name, &gs});
    std::sort(sorted_groups.begin(), sorted_groups.end(), [](const auto& a, const auto& b) {
        double pa = a.second->total_funcs_g > 0
                        ? (100.0 * a.second->covered_funcs_g / a.second->total_funcs_g)
                        : 0;
        double pb = b.second->total_funcs_g > 0
                        ? (100.0 * b.second->covered_funcs_g / b.second->total_funcs_g)
                        : 0;
        return pa < pb;
    });

    // Generate accordion groups
    for (const auto& [name, gs] : sorted_groups) {
        double group_pct =
            gs->total_funcs_g > 0 ? (100.0 * gs->covered_funcs_g / gs->total_funcs_g) : 0;
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
        f << "            <span>" << gs->covered_funcs_g << "/" << gs->total_funcs_g << "</span>\n";
        f << "            <span class=\"status-badge " << badge_class << "\">"
          << gs->submodules.size() << " modules</span>\n";
        f << "          </div>\n";
        f << "        </div>\n";
        f << "        <div class=\"group-content\">\n";

        // Sort submodules by coverage
        std::vector<const ModuleCoverage*> sorted_subs = gs->submodules;
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

            std::string display_name = sub->name;
            if (display_name.find(name + "/") == 0)
                display_name = display_name.substr(name.length() + 1);

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
            f << "            <div class=\"func-list\" style=\"margin-top: 8px; padding-left: "
                 "16px; font-size: 12px;\">\n";
            for (const auto& func : sub->covered_functions)
                f << "              <div style=\"color: var(--green);\">+ " << func << "</div>\n";
            for (const auto& func : sub->uncovered_functions)
                f << "              <div style=\"color: var(--red);\">\u2717 " << func
                  << "</div>\n";
            f << "            </div>\n";
            f << "          </div>\n";
        }

        f << "        </div>\n";
        f << "      </div>\n";
    }

    f << "      </div>\n"; // Close module-groups
    f << "    </div>\n";   // Close modules tab panel

    // Priority section
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

        if (pct == 0.0 && pm.is_critical)
            critical_list.push_back(pm);
        else if (pct == 0.0)
            zero_list.push_back(pm);
        else if (pct < 30.0)
            low_list.push_back(pm);
    }

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

    // Zero coverage
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

    // Low coverage
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

    // Uncovered functions tab
    f << R"(
    <!-- Uncovered Tab -->
    <div id="uncovered" class="tab-panel">
      <h2 class="section-title">Uncovered Functions ()"
      << (total_funcs - total_covered) << R"HTML( total)</h2>
      <div class="search-box">
        <span class="search-icon">&#128269;</span>
        <input type="text" class="search-input" id="uncoveredSearch" placeholder="Filter uncovered modules and functions..." oninput="filterUncovered(this.value)">
        <span class="search-count" id="uncoveredSearchCount"></span>
      </div>
)HTML";

    if (!uncovered_by_module.empty()) {
        std::sort(uncovered_by_module.begin(), uncovered_by_module.end(),
                  [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });

        f << "      <div class=\"uncovered-section\">\n";
        for (const auto& [module_name, funcs] : uncovered_by_module) {
            f << "      <div class=\"uncovered-module\" "
                 "onclick=\"this.classList.toggle('expanded')\">\n";
            f << "        <div class=\"uncovered-header\">\n";
            f << "          <span class=\"module-name\">" << module_name << "</span>\n";
            f << "          <span class=\"uncovered-count\">" << funcs.size()
              << " uncovered</span>\n";
            f << "        </div>\n";
            f << "        <div class=\"uncovered-list\">\n";
            for (const auto& func : funcs)
                f << "          <div class=\"uncovered-func\">" << func << "</div>\n";
            f << "        </div>\n";
            f << "      </div>\n";
        }
        f << "      </div>\n";
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

    for (const auto& suite : stats.suites) {
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
      << stats.total_files << R"(</div>
          <div class="stat-label">Test Files</div>
        </div>
        <div class="stat-card">
          <div class="stat-value">)"
      << tml_suites << R"(</div>
          <div class="stat-label">Test Suites</div>
        </div>
        <div class="stat-card">
          <div class="stat-value">)"
      << stats.total_duration_ms << R"(ms</div>
          <div class="stat-label">Total Duration</div>
        </div>
      </div>
    </div>

    <div class="footer">
      Generated by TML Compiler (v3) &bull; Click on module headers to expand details
    </div>
  </div>

  <script>
    function showTab(tabId) {
      document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.getElementById(tabId).classList.add('active');
      event.target.classList.add('active');
    }

    function filterModules(query) {
      const q = query.toLowerCase().trim();
      const panel = document.getElementById('modules');
      const groups = panel.querySelectorAll('.module-group');
      let visibleCount = 0;

      groups.forEach(group => {
        const title = group.querySelector('.group-title');
        const groupName = title ? title.textContent.toLowerCase() : '';
        const subRows = group.querySelectorAll('.submodule-row');
        let groupMatch = q === '' || groupName.includes(q);
        let anySubVisible = false;

        subRows.forEach(row => {
          const subName = row.querySelector('.submodule-name');
          const funcDivs = row.querySelectorAll('.func-list > div');
          const subText = subName ? subName.textContent.toLowerCase() : '';
          let subMatch = q === '' || groupMatch || subText.includes(q);
          let anyFuncMatch = false;

          funcDivs.forEach(fd => {
            const funcText = fd.textContent.toLowerCase();
            const funcMatch = q === '' || funcText.includes(q);
            fd.style.display = (q === '' || subMatch || funcMatch) ? '' : 'none';
            if (funcMatch && q !== '') anyFuncMatch = true;
          });

          if (anyFuncMatch) subMatch = true;
          row.style.display = subMatch ? '' : 'none';
          if (subMatch) anySubVisible = true;
        });

        const visible = q === '' || groupMatch || anySubVisible;
        group.style.display = visible ? '' : 'none';
        if (visible) visibleCount++;
        if (anySubVisible && q !== '') group.classList.add('expanded');
      });

      const countEl = document.getElementById('moduleSearchCount');
      countEl.textContent = q ? visibleCount + ' group' + (visibleCount !== 1 ? 's' : '') : '';
    }

    function filterUncovered(query) {
      const q = query.toLowerCase().trim();
      const panel = document.getElementById('uncovered');
      const modules = panel.querySelectorAll('.uncovered-module');
      let visibleCount = 0;

      modules.forEach(mod => {
        const modName = mod.querySelector('.module-name');
        const modText = modName ? modName.textContent.toLowerCase() : '';
        const funcs = mod.querySelectorAll('.uncovered-func');
        let modMatch = q === '' || modText.includes(q);
        let anyFuncMatch = false;

        funcs.forEach(fd => {
          const funcText = fd.textContent.toLowerCase();
          const funcMatch = q === '' || funcText.includes(q);
          fd.style.display = (q === '' || modMatch || funcMatch) ? '' : 'none';
          if (funcMatch && q !== '') anyFuncMatch = true;
        });

        const visible = q === '' || modMatch || anyFuncMatch;
        mod.style.display = visible ? '' : 'none';
        if (visible) visibleCount++;
        if ((anyFuncMatch || modMatch) && q !== '') mod.classList.add('expanded');
      });

      const countEl = document.getElementById('uncoveredSearchCount');
      countEl.textContent = q ? visibleCount + ' module' + (visibleCount !== 1 ? 's' : '') : '';
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
        for (const auto& func : mod.functions)
            library_functions.insert(func);
    }

    // Write JSON report
    fs::path json_path = fs::path(output_path).replace_extension(".json");
    std::ofstream json_file(json_path);
    if (json_file.is_open()) {
        json_file << "{\n";
        json_file << "  \"summary\": {\n";
        json_file << "    \"library_functions\": " << total_funcs << ",\n";
        json_file << "    \"library_covered\": " << total_covered << ",\n";
        json_file << "    \"coverage_percent\": " << std::fixed << std::setprecision(2)
                  << overall_pct << ",\n";
        json_file << "    \"modules_full\": " << full_coverage << ",\n";
        json_file << "    \"modules_partial\": " << partial_coverage << ",\n";
        json_file << "    \"modules_zero\": " << zero_coverage << ",\n";
        json_file << "    \"tests_passed\": " << tml_tests << ",\n";
        json_file << "    \"test_files\": " << stats.total_files << ",\n";
        json_file << "    \"duration_ms\": " << stats.total_duration_ms << "\n";
        json_file << "  },\n";

        // Modules sorted by uncovered count descending
        struct ModuleEntry {
            std::string name;
            int total;
            int covered;
            double percent;
            std::vector<std::string> uncovered;
        };
        std::vector<ModuleEntry> sorted_modules;
        for (const auto& mod : modules) {
            if (mod.functions.empty())
                continue;
            ModuleEntry entry;
            entry.name = mod.name;
            entry.total = static_cast<int>(mod.functions.size());
            entry.covered = mod.covered_count;
            entry.percent = entry.total > 0 ? (100.0 * entry.covered / entry.total) : 0.0;
            entry.uncovered = mod.uncovered_functions;
            sorted_modules.push_back(std::move(entry));
        }
        std::sort(sorted_modules.begin(), sorted_modules.end(),
                  [](const ModuleEntry& a, const ModuleEntry& b) {
                      int au = a.total - a.covered, bu = b.total - b.covered;
                      return au != bu ? au > bu : a.name < b.name;
                  });

        json_file << "  \"modules\": [\n";
        for (size_t i = 0; i < sorted_modules.size(); ++i) {
            const auto& m = sorted_modules[i];
            json_file << "    {\n";
            json_file << "      \"name\": \"" << m.name << "\",\n";
            json_file << "      \"total\": " << m.total << ",\n";
            json_file << "      \"covered\": " << m.covered << ",\n";
            json_file << "      \"uncovered\": " << (m.total - m.covered) << ",\n";
            json_file << "      \"percent\": " << std::fixed << std::setprecision(1) << m.percent
                      << ",\n";
            json_file << "      \"uncovered_functions\": [";
            if (!m.uncovered.empty()) {
                json_file << "\n";
                for (size_t j = 0; j < m.uncovered.size(); ++j) {
                    std::string escaped = m.uncovered[j];
                    size_t pos = 0;
                    while ((pos = escaped.find('"', pos)) != std::string::npos) {
                        escaped.replace(pos, 1, "\\\"");
                        pos += 2;
                    }
                    json_file << "        \"" << escaped << "\"";
                    if (j + 1 < m.uncovered.size())
                        json_file << ",";
                    json_file << "\n";
                }
                json_file << "      ";
            }
            json_file << "]\n";
            json_file << "    }";
            if (i + 1 < sorted_modules.size())
                json_file << ",";
            json_file << "\n";
        }
        json_file << "  ]\n}\n";
        json_file.close();
    }

    // Append to coverage history (JSONL)
    {
        fs::path history_path = fs::path(output_path).parent_path() / "coverage_history.jsonl";
        fs::create_directories(history_path.parent_path());
        std::ofstream hf(history_path, std::ios::app);
        if (hf.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto tt = std::chrono::system_clock::to_time_t(now);
            auto ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) %
                1000;
            struct tm tb;
#ifdef _WIN32
            localtime_s(&tb, &tt);
#else
            localtime_r(&tt, &tb);
#endif
            std::ostringstream ts;
            ts << std::put_time(&tb, "%Y-%m-%dT%H:%M:%S") << "." << std::setfill('0')
               << std::setw(3) << ms.count();

            hf << "{\"timestamp\":\"" << ts.str() << "\",\"coverage_percent\":" << std::fixed
               << std::setprecision(2) << overall_pct << ",\"library_functions\":" << total_funcs
               << ",\"library_covered\":" << total_covered
               << ",\"runtime_covered_functions\":" << static_cast<int>(covered_functions.size())
               << ",\"modules_full\":" << full_coverage
               << ",\"modules_partial\":" << partial_coverage
               << ",\"modules_zero\":" << zero_coverage << ",\"tests_passed\":" << tml_tests
               << ",\"tests_failed\":" << stats.failed_count
               << ",\"compilation_errors\":" << stats.compilation_error_count
               << ",\"test_files\":" << stats.total_files
               << ",\"no_cache\":" << (stats.no_cache ? "true" : "false")
               << ",\"duration_ms\":" << stats.total_duration_ms << ",\"modules\":[";

            bool first_mod = true;
            for (const auto& mod : modules) {
                if (mod.functions.empty())
                    continue;
                if (!first_mod)
                    hf << ",";
                first_mod = false;
                hf << "{\"n\":\"" << mod.name
                   << "\",\"t\":" << static_cast<int>(mod.functions.size())
                   << ",\"c\":" << mod.covered_count << "}";
            }
            hf << "]}\n";
            hf.close();

            TML_LOG_INFO("test", "Coverage history appended to " << history_path.string());
        }
    }
}

} // namespace tml::testing
