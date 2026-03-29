TML_MODULE("mcp")

//! # MCP Project Tools
//!
//! Handlers for cache invalidation, project/build, project/coverage,
//! explain, project/structure, project/affected-tests, and project/artifacts.

#include "mcp_tools_internal.hpp"

#include "json/json_parser.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <set>
#include <unordered_map>

namespace tml::mcp {

static auto count_test_files_recursive(const fs::path& dir) -> int {
    int count = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file()) {
            auto filename = entry.path().filename().string();
            if (filename.size() > 9 && filename.substr(filename.size() - 9) == ".test.tml") {
                ++count;
            }
        }
    }
    return count;
}

auto make_project_affected_tests_tool() -> Tool {
    return Tool{
        .name = "project/affected-tests",
        .description = "Detect which test files are affected by recent changes using git diff. "
                       "Maps changed source files to their corresponding test directories.",
        .parameters = {
            {"base", "string", "Git ref to diff against (default: \"HEAD\")", false},
            {"run", "boolean", "Automatically run the affected tests (default: false)", false},
            {"verbose", "boolean", "Show detailed mapping of changes to tests (default: false)",
             false},
        }};
}

auto handle_project_affected_tests(const json::JsonValue& params) -> ToolResult {
    auto root = find_tml_root();
    if (root.empty()) {
        return ToolResult::error("Could not find TML project root. "
                                 "Expected to find lib/core/src/ and lib/std/src/ directories.");
    }

    // Parse parameters
    std::string base_ref = "HEAD";
    auto* base_param = params.get("base");
    if (base_param != nullptr && base_param->is_string()) {
        base_ref = base_param->as_string();
    }

    bool auto_run = false;
    auto* run_param = params.get("run");
    if (run_param != nullptr && run_param->is_bool()) {
        auto_run = run_param->as_bool();
    }

    bool verbose = false;
    auto* verbose_param = params.get("verbose");
    if (verbose_param != nullptr && verbose_param->is_bool()) {
        verbose = verbose_param->as_bool();
    }

    // Run git diff to get changed files
    std::stringstream git_cmd;
#ifdef _WIN32
    git_cmd << "cmd /c \"cd /d " << root.string() << " && git diff --name-only " << base_ref
            << "\"";
#else
    git_cmd << "cd " << root.string() << " && git diff --name-only " << base_ref;
#endif

    auto [diff_output, diff_exit] = execute_command(git_cmd.str());

    // Also get untracked and staged changes
    std::stringstream status_cmd;
#ifdef _WIN32
    status_cmd << "cmd /c \"cd /d " << root.string()
               << " && git diff --name-only --cached && git ls-files --others --exclude-standard\"";
#else
    status_cmd << "cd " << root.string()
               << " && git diff --name-only --cached && git ls-files --others --exclude-standard";
#endif

    auto [status_output, status_exit] = execute_command(status_cmd.str());

    // Combine changed files
    std::set<std::string> changed_files;
    auto parse_lines = [](const std::string& text, std::set<std::string>& out) {
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            // Trim whitespace
            while (!line.empty() &&
                   (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                line.pop_back();
            }
            if (!line.empty()) {
                out.insert(line);
            }
        }
    };

    if (diff_exit == 0)
        parse_lines(diff_output, changed_files);
    if (status_exit == 0)
        parse_lines(status_output, changed_files);

    if (changed_files.empty()) {
        return ToolResult::text("No changes detected (compared to " + base_ref +
                                ").\n"
                                "No tests affected.");
    }

    // Map changed source files to affected test directories
    // Pattern: lib/<lib>/src/<module>/... -> lib/<lib>/tests/<module>/
    std::set<std::string> affected_test_dirs;
    std::set<std::string> affected_modules;
    std::vector<std::pair<std::string, std::string>> mappings; // source -> test dir

    for (const auto& file : changed_files) {
        // Only care about lib/ source files
        if (file.find("lib/") != 0)
            continue;
        if (file.find("/src/") == std::string::npos && file.find("/tests/") == std::string::npos)
            continue;

        // Parse: lib/<lib>/src/<module>/...
        // Extract library name and module
        auto parts_start = 4; // skip "lib/"
        auto lib_end = file.find('/', parts_start);
        if (lib_end == std::string::npos)
            continue;

        std::string lib_name = file.substr(parts_start, lib_end - parts_start);

        auto src_pos = file.find("/src/", lib_end);
        if (src_pos != std::string::npos) {
            // Source file changed - find corresponding test dir
            auto module_start = src_pos + 5; // skip "/src/"
            auto module_end = file.find('/', module_start);

            std::string module_name;
            if (module_end != std::string::npos) {
                module_name = file.substr(module_start, module_end - module_start);
            } else {
                // File directly in src/ - extract name without extension
                module_name = file.substr(module_start);
                auto dot = module_name.rfind('.');
                if (dot != std::string::npos) {
                    module_name = module_name.substr(0, dot);
                }
            }

            if (module_name == "mod")
                continue; // mod.tml maps to all tests in lib

            std::string test_dir = "lib/" + lib_name + "/tests/" + module_name;
            auto full_test_dir = root / test_dir;

            if (fs::exists(full_test_dir) && fs::is_directory(full_test_dir)) {
                affected_test_dirs.insert(test_dir);
                affected_modules.insert(lib_name + "::" + module_name);
                if (verbose) {
                    mappings.push_back({file, test_dir});
                }
            } else {
                // Try broader match — maybe test dir uses different name
                // Check parent tests/ for files matching module name
                auto tests_parent = root / "lib" / lib_name / "tests";
                if (fs::exists(tests_parent)) {
                    std::error_code ec;
                    for (const auto& entry : fs::directory_iterator(tests_parent, ec)) {
                        if (entry.is_directory()) {
                            auto dir_name = entry.path().filename().string();
                            if (dir_name.find(module_name) != std::string::npos) {
                                std::string found_dir = "lib/" + lib_name + "/tests/" + dir_name;
                                affected_test_dirs.insert(found_dir);
                                affected_modules.insert(lib_name + "::" + dir_name);
                                if (verbose) {
                                    mappings.push_back({file, found_dir});
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // Test file itself changed
            auto tests_pos = file.find("/tests/", lib_end);
            if (tests_pos != std::string::npos) {
                auto module_start = tests_pos + 7; // skip "/tests/"
                auto module_end = file.find('/', module_start);
                if (module_end != std::string::npos) {
                    std::string module_name = file.substr(module_start, module_end - module_start);
                    std::string test_dir = "lib/" + lib_name + "/tests/" + module_name;
                    affected_test_dirs.insert(test_dir);
                    affected_modules.insert(lib_name + "::" + module_name);
                }
            }
        }
    }

    // Compiler changes affect everything
    bool compiler_changed = false;
    for (const auto& file : changed_files) {
        if (file.find("compiler/") == 0) {
            compiler_changed = true;
            break;
        }
    }

    // Build result
    std::stringstream result;
    result << "Changed files: " << changed_files.size() << " (vs " << base_ref << ")\n";

    if (compiler_changed) {
        result << "\nCompiler sources changed — all tests may be affected.\n";
    }

    if (affected_test_dirs.empty() && !compiler_changed) {
        result << "\nNo library test directories affected by changes.\n";
        result << "\nChanged files:\n";
        for (const auto& f : changed_files) {
            result << "  " << f << "\n";
        }
        return ToolResult::text(result.str());
    }

    result << "\nAffected modules (" << affected_modules.size() << "):\n";
    for (const auto& mod : affected_modules) {
        result << "  " << mod << "\n";
    }

    result << "\nAffected test directories (" << affected_test_dirs.size() << "):\n";
    for (const auto& dir : affected_test_dirs) {
        int test_count = count_test_files_recursive(root / dir);
        result << "  " << dir << "/ (" << test_count << " test files)\n";
    }

    if (verbose && !mappings.empty()) {
        result << "\nDetailed mappings:\n";
        for (const auto& [src, test] : mappings) {
            result << "  " << src << " -> " << test << "/\n";
        }
    }

    // Auto-run affected tests if requested
    if (auto_run && !affected_test_dirs.empty()) {
        result << "\nRunning affected tests...\n";
        std::string tml_exe = get_tml_executable();

        for (const auto& test_dir : affected_test_dirs) {
            std::stringstream cmd;
            cmd << tml_exe << " test " << (root / test_dir).string();
            auto [test_output, test_exit] = execute_command(cmd.str());

            // Extract summary line
            std::istringstream stream(test_output);
            std::string line;
            std::string summary;
            while (std::getline(stream, line)) {
                if (line.find("test result:") != std::string::npos) {
                    summary = line;
                    // Strip log prefix if present
                    auto tr_pos = summary.find("test result:");
                    if (tr_pos != std::string::npos && tr_pos > 0) {
                        summary = summary.substr(tr_pos);
                    }
                }
            }

            result << "  " << test_dir << ": ";
            if (test_exit == 0) {
                result << "PASS";
            } else {
                result << "FAIL";
            }
            if (!summary.empty()) {
                result << " (" << summary << ")";
            }
            result << "\n";
        }
    }

    return ToolResult::text(result.str());
}

// ============================================================================
// project/artifacts Tool
// ============================================================================

auto make_project_artifacts_tool() -> Tool {
    return Tool{.name = "project/artifacts",
                .description = "List build artifacts: executables, libraries, cache directories, "
                               "and coverage files with size and modification time.",
                .parameters = {
                    {"kind", "string",
                     "Filter by artifact kind: \"executables\", \"libraries\", \"cache\", "
                     "\"coverage\", \"all\" (default: \"all\")",
                     false},
                    {"config", "string",
                     "Build configuration: \"debug\" (default), \"release\", \"all\"", false},
                }};
}

/// Format file size in human-readable form
static auto format_size(uintmax_t bytes) -> std::string {
    if (bytes < 1024)
        return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024)
        return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024ULL * 1024 * 1024)
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / (1024ULL * 1024 * 1024)) + " GB";
}

/// Format filesystem time as relative age
static auto format_age(fs::file_time_type ftime) -> std::string {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - sctp).count();

    if (diff < 60)
        return std::to_string(diff) + "s ago";
    if (diff < 3600)
        return std::to_string(diff / 60) + "m ago";
    if (diff < 86400)
        return std::to_string(diff / 3600) + "h ago";
    return std::to_string(diff / 86400) + "d ago";
}

/// Calculate total size of a directory recursively
static auto dir_size(const fs::path& dir) -> uintmax_t {
    uintmax_t total = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file()) {
            total += entry.file_size(ec);
        }
    }
    return total;
}

auto handle_project_artifacts(const json::JsonValue& params) -> ToolResult {
    auto root = find_tml_root();
    if (root.empty()) {
        return ToolResult::error("Could not find TML project root. "
                                 "Expected to find lib/core/src/ and lib/std/src/ directories.");
    }

    // Parse parameters
    std::string kind = "all";
    auto* kind_param = params.get("kind");
    if (kind_param != nullptr && kind_param->is_string()) {
        kind = kind_param->as_string();
        if (kind != "all" && kind != "executables" && kind != "libraries" && kind != "cache" &&
            kind != "coverage") {
            return ToolResult::error(
                "Invalid kind: \"" + kind +
                "\". Use \"all\", \"executables\", \"libraries\", \"cache\", or \"coverage\".");
        }
    }

    std::string config = "debug";
    auto* config_param = params.get("config");
    if (config_param != nullptr && config_param->is_string()) {
        config = config_param->as_string();
        if (config != "debug" && config != "release" && config != "all") {
            return ToolResult::error("Invalid config: \"" + config +
                                     "\". Use \"debug\", \"release\", or \"all\".");
        }
    }

    std::stringstream result;
    result << "Build Artifacts\n";
    result << "Root: " << root.string() << "\n\n";

    std::error_code ec;

    // Determine which configs to scan
    std::vector<std::string> configs;
    if (config == "all") {
        if (fs::exists(root / "build" / "debug"))
            configs.push_back("debug");
        if (fs::exists(root / "build" / "release"))
            configs.push_back("release");
    } else {
        configs.push_back(config);
    }

    for (const auto& cfg : configs) {
        auto build_dir = root / "build" / cfg;
        if (!fs::exists(build_dir)) {
            result << cfg << "/: (not found)\n\n";
            continue;
        }

        result << cfg << "/\n";

        // Executables
        if (kind == "all" || kind == "executables") {
            result << "  Executables:\n";
            std::vector<std::string> exe_names = {"tml.exe", "tml_mcp.exe", "tml_tests.exe"};
            bool found_any = false;
            for (const auto& name : exe_names) {
                auto path = build_dir / name;
                if (fs::exists(path)) {
                    auto size = fs::file_size(path, ec);
                    auto mtime = fs::last_write_time(path, ec);
                    result << "    " << name << "  " << format_size(size) << "  "
                           << format_age(mtime) << "\n";
                    found_any = true;
                }
            }
            if (!found_any) {
                result << "    (none)\n";
            }
        }

        // Libraries
        if (kind == "all" || kind == "libraries") {
            result << "  Libraries:\n";
            std::vector<std::pair<std::string, fs::path>> libs;

            // Check build dir for .lib files
            for (const auto& entry : fs::directory_iterator(build_dir, ec)) {
                if (entry.is_regular_file() && entry.path().extension() == ".lib") {
                    libs.push_back({entry.path().filename().string(), entry.path()});
                }
            }

            // Check cache dir for .lib files
            auto cache_debug = root / "build" / "cache" / "x86_64-pc-windows-msvc" / cfg / "Debug";
            if (fs::exists(cache_debug)) {
                for (const auto& entry : fs::directory_iterator(cache_debug, ec)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".lib") {
                        libs.push_back({entry.path().filename().string(), entry.path()});
                    }
                }
            }

            std::sort(libs.begin(), libs.end());

            if (libs.empty()) {
                result << "    (none)\n";
            } else {
                uintmax_t total_lib_size = 0;
                for (const auto& [name, path] : libs) {
                    auto size = fs::file_size(path, ec);
                    total_lib_size += size;
                    result << "    " << name << "  " << format_size(size) << "\n";
                }
                result << "    Total: " << format_size(total_lib_size) << " (" << libs.size()
                       << " libraries)\n";
            }
        }

        // Cache directories
        if (kind == "all" || kind == "cache") {
            result << "  Cache:\n";

            struct CacheDir {
                std::string name;
                fs::path path;
            };
            std::vector<CacheDir> cache_dirs = {
                {".run-cache", build_dir / ".run-cache"},
                {"cache", build_dir / "cache"},
                {"cache/meta", build_dir / "cache" / "meta"},
                {"cache/incr", build_dir / "cache" / "incr"},
            };

            // CMake cache
            auto cmake_cache = root / "build" / "cache" / "x86_64-pc-windows-msvc" / cfg;
            if (fs::exists(cmake_cache)) {
                cache_dirs.push_back({"cmake-cache", cmake_cache});
            }

            bool found_any = false;
            for (const auto& cd : cache_dirs) {
                if (fs::exists(cd.path) && fs::is_directory(cd.path)) {
                    auto size = dir_size(cd.path);
                    int file_count = 0;
                    for (const auto& entry : fs::recursive_directory_iterator(cd.path, ec)) {
                        if (entry.is_regular_file())
                            ++file_count;
                    }
                    result << "    " << cd.name << "/  " << format_size(size) << "  (" << file_count
                           << " files)\n";
                    found_any = true;
                }
            }
            if (!found_any) {
                result << "    (none)\n";
            }
        }

        // Coverage files
        if (kind == "all" || kind == "coverage") {
            result << "  Coverage:\n";
            auto cov_dir = root / "build" / "coverage";
            if (fs::exists(cov_dir)) {
                bool found_any = false;
                for (const auto& entry : fs::directory_iterator(cov_dir, ec)) {
                    if (entry.is_regular_file()) {
                        auto name = entry.path().filename().string();
                        auto size = entry.file_size(ec);
                        auto mtime = fs::last_write_time(entry.path(), ec);
                        result << "    " << name << "  " << format_size(size) << "  "
                               << format_age(mtime) << "\n";
                        found_any = true;
                    }
                }
                if (!found_any) {
                    result << "    (no files)\n";
                }
            } else {
                result << "    (not generated — run tests with --coverage)\n";
            }
        }

        result << "\n";
    }

    return ToolResult::text(result.str());
}

// ============================================================================
// project/slow-tests Tool
// ============================================================================

auto make_project_slow_tests_tool() -> Tool {
    return Tool{
        .name = "project/slow-tests",
        .description =
            "Analyze test_log.json to find the slowest individual test files by compilation time. "
            "Parses per-suite and per-file timing data from the last test run.",
        .parameters = {
            {"limit", "number", "Maximum number of slow tests to show (default: 20)", false},
            {"threshold", "number",
             "Only show tests with time above this threshold in ms (default: 0)", false},
            {"sort", "string",
             "Sort by: \"phase1\" (IR gen time, default), \"phase2\" (object compile), \"total\" "
             "(suite total)",
             false},
        }};
}

auto handle_project_slow_tests(const json::JsonValue& params) -> ToolResult {
    auto root = find_tml_root();
    if (root.empty()) {
        return ToolResult::error("Could not find TML project root.");
    }

    auto log_path = root / "build" / "debug" / "test_log.json";
    if (!fs::exists(log_path)) {
        return ToolResult::error(
            "test_log.json not found at: " + log_path.string() +
            "\nRun tests with --verbose --no-cache first to generate the log file.");
    }

    // Parse parameters
    int limit = 20;
    auto* limit_param = params.get("limit");
    if (limit_param != nullptr && limit_param->is_number()) {
        limit = static_cast<int>(limit_param->as_i64());
        if (limit < 1)
            limit = 1;
        if (limit > 500)
            limit = 500;
    }

    int64_t threshold_ms = 0;
    auto* threshold_param = params.get("threshold");
    if (threshold_param != nullptr && threshold_param->is_number()) {
        threshold_ms = threshold_param->as_i64();
    }

    std::string sort_by = "phase1";
    auto* sort_param = params.get("sort");
    if (sort_param != nullptr && sort_param->is_string()) {
        sort_by = sort_param->as_string();
        if (sort_by != "phase1" && sort_by != "phase2" && sort_by != "total") {
            return ToolResult::error("Invalid sort: \"" + sort_by +
                                     "\". Use \"phase1\", \"phase2\", or \"total\".");
        }
    }

    // Read the log file line by line and extract messages
    std::ifstream file(log_path);
    if (!file.is_open()) {
        return ToolResult::error("Could not open: " + log_path.string());
    }

    // ========================================================================
    // Per-file individual timing from "Phase 1 slow" entries (real data)
    // Format: "Phase 1 slow #N: filename.test.tml Xms [lex=A parse=B tc=C borrow=D cg=E]"
    //
    // Suite timing from "Suite <name> timing: ..."
    // Phase 2 per-file from "Phase 2 slow #N: filename.test.tml Xms"
    // ========================================================================

    struct TestFileInfo {
        std::string file_name;
        int64_t total_ms = 0; // total phase1 time for this file
        int64_t lex_ms = 0;
        int64_t parse_ms = 0;
        int64_t tc_ms = 0; // typecheck
        int64_t borrow_ms = 0;
        int64_t cg_ms = 0;      // codegen
        int64_t phase2_ms = 0;  // object compilation time
        std::string suite_name; // which suite this belongs to
    };

    struct SuiteInfo {
        std::string name;
        int64_t phase1_ms = 0;
        int64_t phase2_ms = 0;
        int64_t total_ms = 0;
        int file_count = 0;
    };

    // Map from filename to test info (Phase 1 slow entries)
    std::unordered_map<std::string, TestFileInfo> file_timings;
    // Current suite context: track which suite's Phase 1 slow entries belong to
    std::string current_suite_name;
    // Suite results
    std::vector<SuiteInfo> suites;

    auto extract_time = [](const std::string& msg, const std::string& key) -> int64_t {
        auto pos = msg.find(key + "=");
        if (pos == std::string::npos)
            return 0;
        auto start = pos + key.size() + 1;
        auto end = msg.find_first_not_of("0123456789", start);
        if (end == std::string::npos)
            end = msg.size();
        try {
            return std::stoll(msg.substr(start, end - start));
        } catch (...) {
            return 0;
        }
    };

    // Phase 2 slow entries: temporarily store until we know the suite
    std::vector<std::pair<std::string, int64_t>> pending_phase2;

    std::string line;
    while (std::getline(file, line)) {
        auto msg_pos = line.find("\"msg\":\"");
        if (msg_pos == std::string::npos)
            continue;
        auto start = msg_pos + 7;
        auto end = line.rfind('"');
        if (end <= start)
            continue;
        std::string msg = line.substr(start, end - start);

        // "Phase 1 slow #N: filename.test.tml Xms [lex=A parse=B tc=C borrow=D cg=E]"
        if (msg.find("Phase 1 slow #") == 0) {
            auto colon = msg.find(": ", 14);
            if (colon == std::string::npos)
                continue;
            auto rest = msg.substr(colon + 2);

            // Find the total time: "filename.test.tml 1234ms [..."
            auto ms_pos = rest.find("ms");
            if (ms_pos == std::string::npos)
                continue;

            // Walk back from ms_pos to find the space before the number
            auto space_before_time = rest.rfind(' ', ms_pos);
            if (space_before_time == std::string::npos)
                continue;

            std::string fname = rest.substr(0, space_before_time);
            int64_t total = 0;
            try {
                total =
                    std::stoll(rest.substr(space_before_time + 1, ms_pos - space_before_time - 1));
            } catch (...) {
                continue;
            }

            TestFileInfo tfi;
            tfi.file_name = fname;
            tfi.total_ms = total;

            // Parse sub-phases from brackets: [lex=A parse=B tc=C borrow=D cg=E]
            auto bracket = rest.find('[');
            if (bracket != std::string::npos) {
                auto sub = rest.substr(bracket);
                tfi.lex_ms = extract_time(sub, "lex");
                tfi.parse_ms = extract_time(sub, "parse");
                tfi.tc_ms = extract_time(sub, "tc");
                tfi.borrow_ms = extract_time(sub, "borrow");
                tfi.cg_ms = extract_time(sub, "cg");
            }

            // The suite for this file will be set when we see the Suite timing line
            file_timings[fname] = tfi;
            continue;
        }

        // "Phase 2 slow #N: filename.test.tml Xms"
        if (msg.find("Phase 2 slow #") == 0) {
            auto colon = msg.find(": ", 14);
            if (colon == std::string::npos)
                continue;
            auto rest = msg.substr(colon + 2);
            auto space = rest.rfind(' ');
            if (space == std::string::npos)
                continue;
            std::string fname = rest.substr(0, space);
            std::string time_str = rest.substr(space + 1);
            auto ms_end = time_str.find("ms");
            if (ms_end != std::string::npos)
                time_str = time_str.substr(0, ms_end);
            int64_t ms = 0;
            try {
                ms = std::stoll(time_str);
            } catch (...) {}
            pending_phase2.push_back({fname, ms});
            continue;
        }

        // "Suite <name> timing: preprocess=Nms phase1=Nms phase2=Nms ..."
        if (msg.find("Suite ") == 0 && msg.find(" timing:") != std::string::npos) {
            auto name_end = msg.find(" timing:");
            std::string suite_name = msg.substr(6, name_end - 6);

            SuiteInfo si;
            si.name = suite_name;
            si.phase1_ms = extract_time(msg, "phase1");
            si.phase2_ms = extract_time(msg, "phase2");
            si.total_ms = extract_time(msg, "total");

            // Assign pending phase2 times to file_timings and set suite name
            for (const auto& [fname, ms] : pending_phase2) {
                auto it = file_timings.find(fname);
                if (it != file_timings.end()) {
                    it->second.phase2_ms = ms;
                    it->second.suite_name = suite_name;
                }
            }
            pending_phase2.clear();

            // Also assign suite name to any Phase 1 slow entries that don't have one yet
            // (they were logged just before this Suite timing line)
            // We can't perfectly associate them since logs interleave, but Phase 1 slow
            // entries immediately preceding a Suite timing line belong to that suite.
            // This is handled by Phase 2 slow matching above.

            suites.push_back(std::move(si));
            continue;
        }
    }
    file.close();

    // Build sorted list
    std::vector<TestFileInfo> all_tests;
    all_tests.reserve(file_timings.size());
    for (auto& [_, tfi] : file_timings) {
        all_tests.push_back(std::move(tfi));
    }

    // Apply threshold
    if (threshold_ms > 0) {
        std::vector<TestFileInfo> filtered;
        for (auto& t : all_tests) {
            bool keep = false;
            if (sort_by == "phase1")
                keep = t.total_ms >= threshold_ms;
            else if (sort_by == "phase2")
                keep = t.phase2_ms >= threshold_ms;
            else
                keep = (t.total_ms + t.phase2_ms) >= threshold_ms;
            if (keep)
                filtered.push_back(std::move(t));
        }
        all_tests = std::move(filtered);
    }

    // Sort
    if (sort_by == "phase2") {
        std::sort(all_tests.begin(), all_tests.end(),
                  [](const auto& a, const auto& b) { return a.phase2_ms > b.phase2_ms; });
    } else if (sort_by == "total") {
        std::sort(all_tests.begin(), all_tests.end(), [](const auto& a, const auto& b) {
            return (a.total_ms + a.phase2_ms) > (b.total_ms + b.phase2_ms);
        });
    } else {
        std::sort(all_tests.begin(), all_tests.end(),
                  [](const auto& a, const auto& b) { return a.total_ms > b.total_ms; });
    }

    // Format output
    std::stringstream result;
    result << "=== Slow Tests Analysis (individual per-file timing) ===\n\n";

    // Aggregate stats
    int64_t sum_phase1 = 0, sum_phase2 = 0, sum_total_suite = 0;
    for (const auto& si : suites) {
        sum_phase1 += si.phase1_ms;
        sum_phase2 += si.phase2_ms;
        sum_total_suite += si.total_ms;
    }
    result << "Suites: " << suites.size() << "  |  Test files with timing: " << all_tests.size()
           << "\n";
    result << "Aggregate suite time: " << (sum_total_suite / 1000) << "s"
           << " (phase1=" << (sum_phase1 / 1000) << "s, phase2=" << (sum_phase2 / 1000) << "s)\n\n";

    // Per-test table
    int show_count = std::min(limit, static_cast<int>(all_tests.size()));
    result << "--- Top " << show_count << " Slowest Test Files (sorted by " << sort_by << ") ---\n";
    result << std::left << std::setw(36) << "Test File" << std::right << std::setw(10) << "Total"
           << std::setw(8) << "Lex" << std::setw(8) << "Parse" << std::setw(8) << "TC"
           << std::setw(8) << "Borrow" << std::setw(8) << "Codegen" << std::setw(8) << "Obj"
           << "\n";
    result << std::string(94, '-') << "\n";

    int shown = 0;
    for (const auto& t : all_tests) {
        if (shown >= limit)
            break;

        std::string display_name = t.file_name;
        if (display_name.size() > 34) {
            display_name = "..." + display_name.substr(display_name.size() - 31);
        }

        result << std::left << std::setw(36) << display_name << std::right << std::setw(7)
               << t.total_ms << "ms" << std::setw(6) << t.lex_ms << "ms" << std::setw(6)
               << t.parse_ms << "ms" << std::setw(6) << t.tc_ms << "ms" << std::setw(6)
               << t.borrow_ms << "ms" << std::setw(6) << t.cg_ms << "ms" << std::setw(6)
               << t.phase2_ms << "ms" << "\n";
        ++shown;
    }

    if (all_tests.empty()) {
        result << "\nNo 'Phase 1 slow' entries found in test_log.json.\n";
        result << "Run: tml test --verbose --no-cache\n";
        result << "The log must contain per-file timing data (Phase 1 slow entries).\n";
    }

    return ToolResult::text(result.str());
}

} // namespace tml::mcp
