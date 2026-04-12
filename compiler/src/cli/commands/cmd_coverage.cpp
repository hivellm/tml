TML_MODULE("compiler")

//! # Coverage Command — `tml cv`
//!
//! Project-scoped test coverage report. Reads tml.toml to discover the
//! project boundary, then scans src/ and tests/ directories to build
//! a coverage map of which source modules have corresponding tests.
//!
//! ## Algorithm
//!
//! 1. Load tml.toml from project_path → get package name
//! 2. Scan {project_path}/src/**/*.tml → source modules
//! 3. Scan {project_path}/tests/**/*.test.tml → test files
//! 4. Type-check each file via the query system
//! 5. Count @test functions in each test file
//! 6. Map source modules → test files by checking if tests import the module
//! 7. Print coverage report grouped by subsystem

#include "cmd_coverage.hpp"

#include "../builder/build_config.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

// ANSI colors
namespace cv {
static const char* reset = "\033[0m";
static const char* bold = "\033[1m";
static const char* red = "\033[31m";
static const char* green = "\033[32m";
static const char* yellow = "\033[33m";
static const char* cyan = "\033[36m";
static const char* dim = "\033[2m";
} // namespace cv

// ============================================================================
// File Discovery
// ============================================================================

/// Collect all .tml files under a directory (recursive).
static std::vector<fs::path> find_tml_files(const fs::path& dir,
                                            const std::string& suffix = ".tml") {
    std::vector<fs::path> result;
    if (!fs::exists(dir))
        return result;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            auto name = entry.path().filename().string();
            if (name.size() >= suffix.size() &&
                name.substr(name.size() - suffix.size()) == suffix) {
                result.push_back(entry.path());
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

/// Count @test functions in a TML file.
static int count_test_functions(const fs::path& file) {
    std::ifstream in(file);
    if (!in.is_open())
        return 0;
    int count = 0;
    std::string line;
    while (std::getline(in, line)) {
        // Trim leading whitespace
        auto pos = line.find_first_not_of(" \t");
        if (pos != std::string::npos) {
            auto trimmed = line.substr(pos);
            if (trimmed == "@test" || trimmed.starts_with("@test\r")) {
                count++;
            }
        }
    }
    return count;
}

/// Extract the subsystem name from a source file path.
/// e.g., "compiler-tml/src/types/checker/check_expr.tml" → "types"
static std::string extract_subsystem(const fs::path& file, const fs::path& src_dir) {
    auto rel = fs::relative(file, src_dir);
    // If the file is directly in src/ (no subdirectory), subsystem is "root"
    if (rel.parent_path().empty()) {
        return "root";
    }
    auto first = rel.begin();
    if (first != rel.end()) {
        return first->string();
    }
    return "root";
}

/// Extract module name (stem without .tml) from a file path.
static std::string module_stem(const fs::path& file) {
    auto name = file.filename().string();
    // Strip .test.tml or .tml
    if (name.ends_with(".test.tml")) {
        return name.substr(0, name.size() - 9);
    }
    if (name.ends_with(".tml")) {
        return name.substr(0, name.size() - 4);
    }
    return name;
}

/// Check if a test file references a source module (by checking for the module name in imports).
static bool test_references_module(const fs::path& test_file, const std::string& module_name) {
    std::ifstream in(test_file);
    if (!in.is_open())
        return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("use ") == std::string::npos)
            continue;
        if (line.find(module_name) != std::string::npos)
            return true;
    }
    return false;
}

// ============================================================================
// Type-Check Runner
// ============================================================================

/// Type-check a single file via the TML compiler query system.
/// Returns true if the file passes type-check with zero errors.
/// Get the path to the current tml.exe binary.
static fs::path get_tml_exe() {
    // Try common locations
    auto candidates = {
        fs::path("build/debug/bin/tml.exe"),
        fs::path("build/release/bin/tml.exe"),
        fs::path("tml.exe"),
    };
    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            return fs::absolute(c);
        }
    }
    return fs::path("tml.exe"); // fallback
}

static bool type_check_file(const fs::path& file) {
    static fs::path tml_exe = get_tml_exe();
    std::string cmd =
        "\"" + tml_exe.string() + "\" check \"" + fs::absolute(file).string() + "\" 2>&1";

#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return false;
    }

    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
#endif

    if (status != 0) {
        return false;
    }
    if (output.find("error[") != std::string::npos) {
        return false;
    }
    return true;
}

// ============================================================================
// Report Generation
// ============================================================================

struct SubsystemStats {
    std::string name;
    int total_modules = 0;
    int tested_modules = 0;
    int test_files = 0;
    int test_functions = 0;
    std::vector<std::string> untested_modules;
};

static void print_report(const std::string& package_name, const fs::path& project_path,
                         int src_total, int src_pass, int src_fail, int test_total, int test_pass,
                         int test_fail, int total_test_funcs,
                         const std::map<std::string, SubsystemStats>& subsystems) {
    using namespace cv;

    std::cout << "\n";
    std::cout << bold << "╔══════════════════════════════════════════════════════════╗" << reset
              << "\n";
    std::cout << bold << "║  " << cyan << "tml cv" << reset << bold << " — Coverage Report for "
              << package_name << std::string(std::max(0, 23 - (int)package_name.size()), ' ') << "║"
              << reset << "\n";
    std::cout << bold << "╠══════════════════════════════════════════════════════════╣" << reset
              << "\n";
    std::cout << bold << "║" << reset << "  Project:  " << project_path.string()
              << std::string(std::max(0, 43 - (int)project_path.string().size()), ' ') << bold
              << "║" << reset << "\n";
    std::cout << bold << "║" << reset << "  Sources:  " << green << src_pass << reset << " pass / "
              << src_total << " total";
    if (src_fail > 0)
        std::cout << " (" << red << src_fail << " fail" << reset << ")";
    std::cout << std::string(std::max(0, 30 - (int)std::to_string(src_total).size()), ' ') << bold
              << "║" << reset << "\n";
    std::cout << bold << "║" << reset << "  Tests:    " << green << test_pass << reset << " pass / "
              << test_total << " files, " << bold << total_test_funcs << reset << " @test funcs"
              << std::string(std::max(0, 17 - (int)std::to_string(total_test_funcs).size()), ' ')
              << bold << "║" << reset << "\n";

    // Overall coverage
    int covered = 0, total = 0;
    for (const auto& [_, sub] : subsystems) {
        covered += sub.tested_modules;
        total += sub.total_modules;
    }
    int pct = total > 0 ? (covered * 100 / total) : 0;
    const char* pct_color = pct >= 80 ? green : (pct >= 50 ? yellow : red);
    std::cout << bold << "║" << reset << "  Coverage: " << pct_color << covered << "/" << total
              << " modules (" << pct << "%)" << reset
              << std::string(std::max(0, 32 - (int)std::to_string(pct).size()), ' ') << bold << "║"
              << reset << "\n";
    std::cout << bold << "╠══════════════════════════════════════════════════════════╣" << reset
              << "\n";

    // Per-subsystem table
    std::cout << bold << "║" << reset << "  " << bold << std::left << std::setw(16) << "Subsystem"
              << std::right << std::setw(8) << "Modules" << std::setw(8) << "Tested" << std::setw(8)
              << "@test" << std::setw(10) << "Coverage" << reset << "  " << bold << "║" << reset
              << "\n";
    std::cout << bold << "║" << reset << "  " << dim << std::left << std::setw(16)
              << "────────────────" << std::right << std::setw(8) << "────────" << std::setw(8)
              << "────────" << std::setw(8) << "────────" << std::setw(10) << "──────────" << reset
              << "  " << bold << "║" << reset << "\n";

    for (const auto& [name, sub] : subsystems) {
        int sub_pct = sub.total_modules > 0 ? (sub.tested_modules * 100 / sub.total_modules) : 0;
        const char* sc = sub_pct >= 80 ? green : (sub_pct >= 50 ? yellow : red);

        std::cout << bold << "║" << reset << "  " << std::left << std::setw(16) << name
                  << std::right << std::setw(8) << sub.total_modules << std::setw(8)
                  << sub.tested_modules << std::setw(8) << sub.test_functions << "  " << sc
                  << std::setw(6) << std::to_string(sub_pct) + "%" << reset << "    " << bold << "║"
                  << reset << "\n";
    }

    std::cout << bold << "╚══════════════════════════════════════════════════════════╝" << reset
              << "\n";

    // List untested modules
    bool has_untested = false;
    for (const auto& [_, sub] : subsystems) {
        if (!sub.untested_modules.empty())
            has_untested = true;
    }
    if (has_untested) {
        std::cout << "\n" << yellow << "Untested modules:" << reset << "\n";
        for (const auto& [name, sub] : subsystems) {
            for (const auto& mod : sub.untested_modules) {
                std::cout << "  " << dim << name << "/" << reset << mod << "\n";
            }
        }
    }
    std::cout << "\n";
}

// ============================================================================
// Main Entry Point
// ============================================================================

int run_coverage(int argc, char* argv[], bool verbose) {
    CoverageOptions opts;
    opts.verbose = verbose;

    // Parse args
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--quick")
            opts.quick = true;
        else if (arg == "--verbose" || arg == "-v")
            opts.verbose = true;
        else if (arg == "--json")
            opts.json = true;
        else if (arg.starts_with("--path="))
            opts.project_path = arg.substr(7);
        else if (arg[0] != '-')
            opts.project_path = arg;
    }

    fs::path project_path = fs::path(opts.project_path);
    fs::path manifest_path = project_path / "tml.toml";

    // Load manifest
    if (!fs::exists(manifest_path)) {
        std::cerr << cv::red << "Error: " << cv::reset << "No tml.toml found at " << manifest_path
                  << "\n";
        return 1;
    }

    auto manifest = Manifest::load(manifest_path);
    if (!manifest) {
        std::cerr << cv::red << "Error: " << cv::reset << "Failed to parse " << manifest_path
                  << "\n";
        return 1;
    }

    std::string package_name = manifest->package.name;

    // Discover files
    fs::path src_dir = project_path / "src";
    fs::path test_dir = project_path / "tests";

    auto src_files = find_tml_files(src_dir, ".tml");
    auto test_files = find_tml_files(test_dir, ".test.tml");

    if (opts.verbose) {
        std::cout << cv::cyan << "Project: " << cv::reset << package_name << " (" << project_path
                  << ")\n";
        std::cout << cv::cyan << "Sources: " << cv::reset << src_files.size() << " files in "
                  << src_dir << "\n";
        std::cout << cv::cyan << "Tests:   " << cv::reset << test_files.size() << " files in "
                  << test_dir << "\n\n";
    }

    // Phase 1: Type-check source files
    int src_pass = 0, src_fail = 0;
    for (const auto& f : src_files) {
        bool ok = type_check_file(f);
        if (ok) {
            src_pass++;
        } else {
            src_fail++;
            if (opts.verbose) {
                std::cout << "  " << cv::red << "FAIL" << cv::reset << ": " << f << "\n";
            }
        }
    }

    // Phase 2: Type-check test files + count @test
    int test_pass = 0, test_fail = 0, total_test_funcs = 0;
    std::map<std::string, int> test_func_counts; // file → count
    for (const auto& f : test_files) {
        bool ok = type_check_file(f);
        if (ok) {
            test_pass++;
        } else {
            test_fail++;
            if (opts.verbose) {
                std::cout << "  " << cv::red << "FAIL" << cv::reset << ": " << f << "\n";
            }
        }
        int tc = count_test_functions(f);
        test_func_counts[f.string()] = tc;
        total_test_funcs += tc;
    }

    // Phase 3: Map source modules → tests
    std::map<std::string, SubsystemStats> subsystems;

    for (const auto& src : src_files) {
        auto subsystem = extract_subsystem(src, src_dir);
        auto mod_name = module_stem(src);

        subsystems[subsystem].name = subsystem;
        subsystems[subsystem].total_modules++;

        if (!opts.quick) {
            bool has_test = false;
            for (const auto& tf : test_files) {
                if (test_references_module(tf, mod_name)) {
                    has_test = true;
                    break;
                }
            }
            if (has_test) {
                subsystems[subsystem].tested_modules++;
            } else {
                subsystems[subsystem].untested_modules.push_back(mod_name);
            }
        }
    }

    // Count test functions per subsystem
    for (const auto& tf : test_files) {
        auto subsystem = extract_subsystem(tf, test_dir);
        subsystems[subsystem].test_files++;
        subsystems[subsystem].test_functions += test_func_counts[tf.string()];
    }

    // Print report
    print_report(package_name, project_path, (int)src_files.size(), src_pass, src_fail,
                 (int)test_files.size(), test_pass, test_fail, total_test_funcs, subsystems);

    return (src_fail > 0 || test_fail > 0) ? 1 : 0;
}

} // namespace tml::cli
