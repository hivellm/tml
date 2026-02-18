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

// ============================================================================
// Cache Invalidation Tool
// ============================================================================

auto make_cache_invalidate_tool() -> Tool {
    return Tool{.name = "cache/invalidate",
                .description =
                    "Invalidate cache for specific source files. Forces full recompilation on "
                    "next build. Use this when cached results are stale.",
                .parameters = {
                    {"files", "array", "List of file paths to invalidate cache for", true},
                    {"verbose", "boolean", "Show detailed output about invalidated entries", false},
                }};
}

auto handle_cache_invalidate(const json::JsonValue& params) -> ToolResult {
    // Get files parameter (required)
    auto* files_param = params.get("files");
    if (files_param == nullptr || !files_param->is_array()) {
        return ToolResult::error(
            "Missing or invalid 'files' parameter (expected array of strings)");
    }

    std::vector<std::string> files;
    for (const auto& file : files_param->as_array()) {
        if (file.is_string()) {
            files.push_back(file.as_string());
        }
    }

    if (files.empty()) {
        return ToolResult::error("No valid file paths provided in 'files' array");
    }

    // Get verbose parameter (optional)
    bool verbose = false;
    auto* verbose_param = params.get("verbose");
    if (verbose_param != nullptr && verbose_param->is_bool()) {
        verbose = verbose_param->as_bool();
    }

    // Build command - use the TML executable for cache invalidation
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " cache invalidate";

    if (verbose) {
        cmd << " --verbose";
    }

    // Add files
    for (const auto& file : files) {
        cmd << " \"" << file << "\"";
    }

    // Execute
    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Cache invalidation successful!\n";
        result << "Files processed: " << files.size() << "\n";
    } else {
        result << "Cache invalidation completed with warnings (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    // Provide guidance
    result << "\nNext build will recompile these files from scratch.\n";

    return ToolResult::text(result.str());
}

// ============================================================================
// Project Build Tool
// ============================================================================

auto make_project_build_tool() -> Tool {
    return Tool{.name = "project/build",
                .description =
                    "Build the TML compiler from C++ sources using project build scripts. "
                    "Eliminates the need for complex shell commands with path escaping.",
                .parameters = {
                    {"mode", "string", "Build mode: \"debug\" (default) or \"release\"", false},
                    {"clean", "boolean", "Clean build directory first", false},
                    {"tests", "boolean", "Build test executable (default: true)", false},
                    {"target", "string",
                     "Build target: \"all\" (default), \"compiler\" (tml.exe only), "
                     "\"mcp\" (tml_mcp.exe only). Use \"compiler\" to update tml.exe "
                     "without rebuilding the running MCP server.",
                     false},
                }};
}

auto handle_project_build(const json::JsonValue& params) -> ToolResult {
    // Discover project root
    auto root = find_tml_root();
    if (root.empty()) {
        return ToolResult::error("Could not find TML project root. "
                                 "Expected to find lib/core/src/ and lib/std/src/ directories.");
    }

    // Parse parameters
    std::string mode = "debug";
    auto* mode_param = params.get("mode");
    if (mode_param != nullptr && mode_param->is_string()) {
        mode = mode_param->as_string();
        if (mode != "debug" && mode != "release") {
            return ToolResult::error("Invalid mode: \"" + mode +
                                     "\". Use \"debug\" or \"release\".");
        }
    }

    bool clean = false;
    auto* clean_param = params.get("clean");
    if (clean_param != nullptr && clean_param->is_bool()) {
        clean = clean_param->as_bool();
    }

    bool build_tests = true;
    auto* tests_param = params.get("tests");
    if (tests_param != nullptr && tests_param->is_bool()) {
        build_tests = tests_param->as_bool();
    }

    // Parse target: "all" (default), "compiler" (tml.exe only), "mcp" (tml_mcp.exe only)
    std::string target = "all";
    auto* target_param = params.get("target");
    if (target_param != nullptr && target_param->is_string()) {
        target = target_param->as_string();
        if (target != "all" && target != "compiler" && target != "mcp") {
            return ToolResult::error(
                "Invalid target: \"" + target +
                "\". Use \"all\", \"compiler\" (tml.exe), or \"mcp\" (tml_mcp.exe).");
        }
    }

    // Map target names to CMake target names
    std::string cmake_target;
    if (target == "compiler") {
        cmake_target = "tml";
    } else if (target == "mcp") {
        cmake_target = "tml_mcp";
    }
    // "all" → no --target flag (build everything)

    // Build the command
    std::stringstream cmd;

#ifdef _WIN32
    auto build_script = root / "scripts" / "build.bat";
    if (!fs::exists(build_script)) {
        return ToolResult::error("Build script not found: " + build_script.string());
    }

    // Use cmd /c to execute the batch file with proper working directory
    cmd << "cmd /c \"cd /d " << root.string() << " && scripts\\build.bat";

    if (mode == "release") {
        cmd << " release";
    }
    if (clean) {
        cmd << " --clean";
    }
    if (!build_tests) {
        cmd << " --no-tests";
    }
    if (!cmake_target.empty()) {
        cmd << " --target " << cmake_target;
    }
    cmd << "\"";
#else
    auto build_script = root / "scripts" / "build.sh";
    if (!fs::exists(build_script)) {
        // Fall back to build.bat via bash
        build_script = root / "scripts" / "build.bat";
    }
    if (!fs::exists(build_script)) {
        return ToolResult::error("Build script not found in: " + (root / "scripts").string());
    }

    cmd << "cd " << root.string() << " && bash " << build_script.string();

    if (mode == "release") {
        cmd << " release";
    }
    if (clean) {
        cmd << " --clean";
    }
    if (!build_tests) {
        cmd << " --no-tests";
    }
    if (!cmake_target.empty()) {
        cmd << " --target " << cmake_target;
    }
#endif

    // Execute the build using CreateProcess (Windows) or fork/exec (Unix)
    // to isolate the MCP server from build crashes/hangs.
    // Output is captured via a temp file to avoid pipe buffer deadlocks.
    auto start = std::chrono::steady_clock::now();
    std::string output;
    int exit_code = -1;
    constexpr int timeout_seconds = 300;

#ifdef _WIN32
    // Create a temp file for capturing build output
    char temp_dir[MAX_PATH];
    char temp_file[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    GetTempFileNameA(temp_dir, "tml", 0, temp_file);
    std::string temp_path(temp_file);

    // Set up security attributes for handle inheritance
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    // Open temp file for writing (inheritable handle)
    HANDLE hFile = CreateFileA(temp_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return ToolResult::error("Failed to create temp file for build output.");
    }

    // Set up process startup info — redirect stdout+stderr to temp file
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hFile;
    si.hStdError = hFile;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};

    // Build command line — must be mutable for CreateProcessA
    std::string cmd_line = cmd.str() + " 2>&1";
    std::vector<char> cmd_buf(cmd_line.begin(), cmd_line.end());
    cmd_buf.push_back('\0');

    // Launch the build subprocess
    BOOL created = CreateProcessA(nullptr,               // lpApplicationName
                                  cmd_buf.data(),        // lpCommandLine (mutable)
                                  nullptr,               // lpProcessAttributes
                                  nullptr,               // lpThreadAttributes
                                  TRUE,                  // bInheritHandles
                                  CREATE_NO_WINDOW,      // dwCreationFlags
                                  nullptr,               // lpEnvironment (inherit)
                                  root.string().c_str(), // lpCurrentDirectory
                                  &si,                   // lpStartupInfo
                                  &pi                    // lpProcessInformation
    );

    if (!created) {
        CloseHandle(hFile);
        DeleteFileA(temp_path.c_str());
        DWORD err = GetLastError();
        return ToolResult::error("Failed to launch build process (error " + std::to_string(err) +
                                 ").\nCommand: " + cmd.str());
    }

    // Wait for the process with timeout
    DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_seconds * 1000);

    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000); // Wait for termination
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hFile);
        // Read whatever output was captured
        std::ifstream tf(temp_path);
        if (tf.is_open()) {
            std::ostringstream oss;
            oss << tf.rdbuf();
            output = strip_ansi(oss.str());
            tf.close();
        }
        DeleteFileA(temp_path.c_str());
        return ToolResult::error("Build timed out after " + std::to_string(timeout_seconds) +
                                 "s.\n\n--- Partial Output ---\n" + output);
    }

    // Get exit code
    DWORD dwExitCode = 0;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    exit_code = static_cast<int>(dwExitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hFile);

    // Read captured output from temp file
    {
        std::ifstream tf(temp_path);
        if (tf.is_open()) {
            std::ostringstream oss;
            oss << tf.rdbuf();
            output = strip_ansi(oss.str());
            tf.close();
        }
    }
    DeleteFileA(temp_path.c_str());

#else
    // Unix: use popen as before (safer on Unix — no self-replacing binary issue)
    auto result_pair = execute_command(cmd.str(), timeout_seconds);
    output = result_pair.first;
    exit_code = result_pair.second;
#endif

    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Format result
    std::stringstream result;
    if (exit_code == 0) {
        result << "Build successful! (" << mode << " mode, " << duration_ms << "ms)\n";
        result << "Project root: " << root.string() << "\n";

        // Try to find the built executable
        auto exe_path = root / "build" / mode / "tml.exe";
        if (fs::exists(exe_path)) {
            result << "Output: " << exe_path.string() << "\n";
            auto file_size = fs::file_size(exe_path);
            result << "Size: " << (file_size / 1024 / 1024) << " MB\n";
        }
    } else {
        result << "Build failed! (exit code " << exit_code << ", " << duration_ms << "ms)\n";
        result << "Mode: " << mode << "\n";
        result << "Project root: " << root.string() << "\n";
    }

    if (!output.empty()) {
        // Truncate output if too large to avoid overwhelming MCP response
        constexpr size_t max_output = 32000;
        if (output.size() > max_output) {
            auto truncated = output.substr(0, 4000) + "\n\n... [" +
                             std::to_string(output.size() - 8000) + " bytes truncated] ...\n\n" +
                             output.substr(output.size() - 4000);
            result << "\n--- Build Output ---\n" << truncated;
        } else {
            result << "\n--- Build Output ---\n" << output;
        }
    }

    if (exit_code != 0) {
        return ToolResult::error(result.str());
    }

    return ToolResult::text(result.str());
}

// ============================================================================
// Project Coverage Tool
// ============================================================================

auto make_project_coverage_tool() -> Tool {
    return Tool{
        .name = "project/coverage",
        .description = "Read and return structured coverage data from the last test run. "
                       "Parses build/coverage/coverage.json for library function coverage stats.",
        .parameters = {
            {"module", "string", "Filter to specific module (e.g., \"core::str\", \"std::json\")",
             false},
            {"sort", "string", "Sort order: \"lowest\" (default), \"name\", \"highest\"", false},
            {"limit", "number", "Maximum number of modules to return", false},
            {"refresh", "boolean", "Run tests with --coverage first to generate fresh data", false},
        }};
}

auto handle_project_coverage(const json::JsonValue& params) -> ToolResult {
    auto root = find_tml_root();
    if (root.empty()) {
        return ToolResult::error("Could not find TML project root. "
                                 "Expected to find lib/core/src/ and lib/std/src/ directories.");
    }

    // Check if refresh is requested
    auto* refresh_param = params.get("refresh");
    if (refresh_param != nullptr && refresh_param->is_bool() && refresh_param->as_bool()) {
        // Run tests with coverage to generate fresh data
        std::string tml_exe = get_tml_executable();
        std::stringstream cmd;
        cmd << tml_exe << " test --coverage --no-cache";
        auto [output, exit_code] = execute_command(cmd.str());
        if (exit_code != 0) {
            return ToolResult::error("Failed to run tests with coverage:\n" + output);
        }
    }

    // Read coverage.json
    auto coverage_path = root / "build" / "coverage" / "coverage.json";
    if (!fs::exists(coverage_path)) {
        return ToolResult::error("Coverage data not found at: " + coverage_path.string() +
                                 "\nRun tests with --coverage first, or use refresh: true.");
    }

    std::ifstream file(coverage_path);
    if (!file.is_open()) {
        return ToolResult::error("Could not open coverage file: " + coverage_path.string());
    }

    std::string json_content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    file.close();

    // Parse JSON
    auto parse_result = json::parse_json(json_content);
    if (is_err(parse_result)) {
        return ToolResult::error("Failed to parse coverage JSON: " +
                                 unwrap_err(parse_result).message);
    }

    const auto& data = unwrap(parse_result);

    std::stringstream result;
    result << "=== TML Library Coverage Report ===\n\n";

    // Read summary from nested "summary" object
    const auto* summary = data.get("summary");
    if (summary == nullptr || !summary->is_object()) {
        return ToolResult::error("Coverage JSON missing 'summary' object. "
                                 "The coverage.json format may have changed. "
                                 "Re-run tests with --coverage to regenerate.");
    }

    auto get_int = [&](const json::JsonValue& obj, const char* key) -> int {
        auto* v = obj.get(key);
        return (v && v->is_number()) ? static_cast<int>(v->as_i64()) : 0;
    };
    auto get_double = [&](const json::JsonValue& obj, const char* key) -> double {
        auto* v = obj.get(key);
        return (v && v->is_number()) ? v->as_f64() : 0.0;
    };

    int lib_funcs = get_int(*summary, "library_functions");
    int lib_covered = get_int(*summary, "library_covered");
    double lib_pct = get_double(*summary, "coverage_percent");
    int tests_passed = get_int(*summary, "tests_passed");
    int test_files = get_int(*summary, "test_files");
    int duration_ms = get_int(*summary, "duration_ms");
    int mods_full = get_int(*summary, "modules_full");
    int mods_partial = get_int(*summary, "modules_partial");
    int mods_zero = get_int(*summary, "modules_zero");

    result << std::fixed << std::setprecision(1);
    result << "Library Coverage: " << lib_covered << "/" << lib_funcs << " functions (" << lib_pct
           << "%)\n";
    result << "Tests: " << tests_passed << " passed across " << test_files << " files\n";
    result << "Duration: " << duration_ms << "ms\n";
    result << "Modules: " << mods_full << " at 100%, " << mods_partial << " partial, " << mods_zero
           << " at 0%\n\n";

    // Read per-module data from "modules" array
    const auto* modules = data.get("modules");
    if (modules != nullptr && modules->is_array()) {
        // Apply module filter if specified
        std::string filter_str;
        auto* module_filter = params.get("module");
        if (module_filter != nullptr && module_filter->is_string()) {
            filter_str = module_filter->as_string();
            // Normalize :: to / for matching against module names in JSON
            size_t pos = 0;
            while ((pos = filter_str.find("::", pos)) != std::string::npos) {
                filter_str.replace(pos, 2, "/");
            }
        }

        // Collect modules into a sortable structure
        struct ModEntry {
            std::string name;
            int covered;
            int total;
            double pct;
            std::vector<std::string> uncovered;
        };

        std::vector<ModEntry> entries;
        const auto& mod_arr = modules->as_array();
        for (size_t i = 0; i < mod_arr.size(); ++i) {
            const auto& m = mod_arr[i];
            auto* mname = m.get("name");
            if (mname == nullptr || !mname->is_string())
                continue;

            std::string name = mname->as_string();

            // Apply filter
            if (!filter_str.empty() && name.find(filter_str) == std::string::npos)
                continue;

            ModEntry entry;
            entry.name = name;
            entry.total = get_int(m, "total");
            entry.covered = get_int(m, "covered");
            entry.pct = get_double(m, "percent");

            // Collect uncovered function names
            auto* uncov = m.get("uncovered_functions");
            if (uncov != nullptr && uncov->is_array()) {
                const auto& uncov_arr = uncov->as_array();
                for (size_t j = 0; j < uncov_arr.size(); ++j) {
                    if (uncov_arr[j].is_string()) {
                        entry.uncovered.push_back(uncov_arr[j].as_string());
                    }
                }
            }

            entries.push_back(std::move(entry));
        }

        // Sort
        std::string sort_order = "lowest";
        auto* sort_param = params.get("sort");
        if (sort_param != nullptr && sort_param->is_string()) {
            sort_order = sort_param->as_string();
        }

        if (sort_order == "name") {
            std::sort(entries.begin(), entries.end(),
                      [](const auto& a, const auto& b) { return a.name < b.name; });
        } else if (sort_order == "highest") {
            std::sort(entries.begin(), entries.end(),
                      [](const auto& a, const auto& b) { return a.pct > b.pct; });
        } else { // lowest (default)
            std::sort(entries.begin(), entries.end(),
                      [](const auto& a, const auto& b) { return a.pct < b.pct; });
        }

        // Apply limit
        int limit = static_cast<int>(entries.size());
        auto* limit_param = params.get("limit");
        if (limit_param != nullptr && limit_param->is_number()) {
            int requested = static_cast<int>(limit_param->as_i64());
            if (requested > 0 && requested < limit) {
                limit = requested;
            }
        }

        // Output per-module breakdown
        result << "--- Per-Module Coverage ---\n";
        result << std::left << std::setw(30) << "Module" << std::right << std::setw(10) << "Covered"
               << std::setw(10) << "Total" << std::setw(10) << "Pct" << "\n";
        result << std::string(60, '-') << "\n";

        for (int i = 0; i < limit && i < static_cast<int>(entries.size()); ++i) {
            const auto& mod = entries[i];
            result << std::left << std::setw(30) << mod.name << std::right << std::setw(10)
                   << mod.covered << std::setw(10) << mod.total << std::setw(9) << std::fixed
                   << std::setprecision(1) << mod.pct << "%\n";

            // When filtering to a specific module, show uncovered functions
            if (!filter_str.empty() && !mod.uncovered.empty()) {
                result << "  Uncovered functions:\n";
                for (const auto& fn : mod.uncovered) {
                    result << "    - " << fn << "\n";
                }
            }
        }

        if (limit < static_cast<int>(entries.size())) {
            result << "... and " << (entries.size() - limit) << " more modules\n";
        }
    }

    return ToolResult::text(result.str());
}

// ============================================================================
// Explain Tool
// ============================================================================

auto make_explain_tool() -> Tool {
    return Tool{.name = "explain",
                .description = "Show detailed explanation for a TML compiler error code. "
                               "Returns error description, common causes, and fix examples.",
                .parameters = {
                    {"code", "string", "Error code (e.g., \"T001\", \"B001\", \"L003\")", true},
                }};
}

auto handle_explain(const json::JsonValue& params) -> ToolResult {
    auto* code_param = params.get("code");
    if (code_param == nullptr || !code_param->is_string()) {
        return ToolResult::error("Missing or invalid 'code' parameter (expected string)");
    }

    std::string code = code_param->as_string();

    // Invoke the tml explain command
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " explain " << code;

    auto [output, exit_code] = execute_command(cmd.str());

    if (exit_code != 0) {
        if (output.empty()) {
            return ToolResult::error("Unknown error code: " + code);
        }
        // The explain command prints helpful error messages (similar codes, categories)
        return ToolResult::error(output);
    }

    return ToolResult::text(output);
}

// ============================================================================
// project/structure Tool
// ============================================================================

auto make_project_structure_tool() -> Tool {
    return Tool{
        .name = "project/structure",
        .description =
            "Show the TML project module tree with file counts and test coverage. "
            "Uses std::filesystem to enumerate lib/ subdirectories without shell commands.",
        .parameters = {
            {"module", "string",
             "Filter to specific library or module (e.g., \"core\", \"std::json\", \"test\")",
             false},
            {"depth", "number", "Maximum directory depth to display (default: 3)", false},
            {"show_files", "boolean",
             "Show individual file names instead of just counts (default: false)", false},
        }};
}

/// Count .tml files recursively
static auto count_tml_files_recursive(const fs::path& dir) -> int {
    int count = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".tml") {
            ++count;
        }
    }
    return count;
}

/// Count test files (*.test.tml) recursively
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

/// Build module tree for a subdirectory
static void build_subtree(std::stringstream& out, const fs::path& dir, const std::string& prefix,
                          int depth, int max_depth, bool show_files) {
    if (depth >= max_depth)
        return;

    std::error_code ec;
    std::vector<fs::path> subdirs;
    std::vector<std::string> files;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_directory()) {
            subdirs.push_back(entry.path());
        } else if (show_files && entry.is_regular_file() && entry.path().extension() == ".tml") {
            files.push_back(entry.path().filename().string());
        }
    }

    std::sort(subdirs.begin(), subdirs.end());
    std::sort(files.begin(), files.end());

    // Print files first
    for (const auto& f : files) {
        out << prefix << "  " << f << "\n";
    }

    // Then subdirectories
    for (size_t i = 0; i < subdirs.size(); ++i) {
        auto name = subdirs[i].filename().string();
        int file_count = count_tml_files_recursive(subdirs[i]);
        bool is_last = (i == subdirs.size() - 1) && files.empty();

        out << prefix << (is_last ? "└── " : "├── ") << name << "/";
        if (file_count > 0) {
            out << " (" << file_count << " files)";
        }
        out << "\n";

        std::string next_prefix = prefix + (is_last ? "    " : "│   ");
        build_subtree(out, subdirs[i], next_prefix, depth + 1, max_depth, show_files);
    }
}

auto handle_project_structure(const json::JsonValue& params) -> ToolResult {
    auto root = find_tml_root();
    if (root.empty()) {
        return ToolResult::error("Could not find TML project root. "
                                 "Expected to find lib/core/src/ and lib/std/src/ directories.");
    }

    // Parse parameters
    std::string module_filter;
    auto* module_param = params.get("module");
    if (module_param != nullptr && module_param->is_string()) {
        module_filter = module_param->as_string();
    }

    int max_depth = 3;
    auto* depth_param = params.get("depth");
    if (depth_param != nullptr && depth_param->is_number()) {
        max_depth = static_cast<int>(depth_param->as_i64());
        if (max_depth < 1)
            max_depth = 1;
        if (max_depth > 10)
            max_depth = 10;
    }

    bool show_files = false;
    auto* files_param = params.get("show_files");
    if (files_param != nullptr && files_param->is_bool()) {
        show_files = files_param->as_bool();
    }

    auto lib_dir = root / "lib";
    if (!fs::exists(lib_dir)) {
        return ToolResult::error("lib/ directory not found at: " + lib_dir.string());
    }

    std::stringstream result;

    // Discover all libraries in lib/
    std::error_code ec;
    std::vector<fs::path> libraries;
    for (const auto& entry : fs::directory_iterator(lib_dir, ec)) {
        if (entry.is_directory()) {
            libraries.push_back(entry.path());
        }
    }
    std::sort(libraries.begin(), libraries.end());

    // If module filter is set, narrow down
    if (!module_filter.empty()) {
        // Extract library name (first part before ::)
        std::string lib_name = module_filter;
        std::string sub_module;
        auto sep = module_filter.find("::");
        if (sep != std::string::npos) {
            lib_name = module_filter.substr(0, sep);
            sub_module = module_filter.substr(sep + 2);
            // Replace :: with / for path
            for (size_t pos = 0; (pos = sub_module.find("::", pos)) != std::string::npos;) {
                sub_module.replace(pos, 2, "/");
            }
        }

        auto lib_path = lib_dir / lib_name;
        if (!fs::exists(lib_path)) {
            return ToolResult::error("Library not found: " + lib_name +
                                     "\nAvailable libraries: " + [&]() {
                                         std::string libs;
                                         for (const auto& l : libraries) {
                                             if (!libs.empty())
                                                 libs += ", ";
                                             libs += l.filename().string();
                                         }
                                         return libs;
                                     }());
        }

        // Show filtered library
        result << "Module: " << module_filter << "\n\n";

        auto src_dir = lib_path / "src";
        auto tests_dir = lib_path / "tests";

        if (!sub_module.empty()) {
            // Show specific sub-module
            auto sub_src = src_dir / sub_module;
            auto sub_tests = tests_dir / sub_module;

            if (fs::exists(sub_src)) {
                int src_count = count_tml_files_recursive(sub_src);
                result << "src/" << sub_module << "/ (" << src_count << " files)\n";
                build_subtree(result, sub_src, "  ", 0, max_depth, show_files);
            }
            // Also check for single file
            auto sub_src_file = src_dir / (sub_module + ".tml");
            if (fs::exists(sub_src_file)) {
                result << "src/" << sub_module << ".tml\n";
            }

            if (fs::exists(sub_tests)) {
                int test_count = count_test_files_recursive(sub_tests);
                result << "tests/" << sub_module << "/ (" << test_count << " test files)\n";
                build_subtree(result, sub_tests, "  ", 0, max_depth, show_files);
            }
        } else {
            // Show entire library
            int src_count = fs::exists(src_dir) ? count_tml_files_recursive(src_dir) : 0;
            int test_count = fs::exists(tests_dir) ? count_test_files_recursive(tests_dir) : 0;

            result << "src/ (" << src_count << " source files)\n";
            if (fs::exists(src_dir)) {
                build_subtree(result, src_dir, "  ", 0, max_depth, show_files);
            }

            result << "tests/ (" << test_count << " test files)\n";
            if (fs::exists(tests_dir)) {
                build_subtree(result, tests_dir, "  ", 0, max_depth, show_files);
            }
        }

        return ToolResult::text(result.str());
    }

    // Full project overview
    result << "TML Project Structure\n";
    result << "Root: " << root.string() << "\n\n";

    int total_src = 0, total_tests = 0;

    for (const auto& lib_path : libraries) {
        auto name = lib_path.filename().string();
        auto src_dir = lib_path / "src";
        auto tests_dir = lib_path / "tests";

        int src_count = fs::exists(src_dir) ? count_tml_files_recursive(src_dir) : 0;
        int test_count = fs::exists(tests_dir) ? count_test_files_recursive(tests_dir) : 0;
        total_src += src_count;
        total_tests += test_count;

        result << "lib/" << name << "/\n";
        if (fs::exists(src_dir)) {
            result << "  src/ (" << src_count << " source files)\n";
            build_subtree(result, src_dir, "    ", 0, max_depth - 1, show_files);
        }
        if (fs::exists(tests_dir)) {
            result << "  tests/ (" << test_count << " test files)\n";
            build_subtree(result, tests_dir, "    ", 0, max_depth - 1, show_files);
        }

        // Check for other dirs (runtime, docs, examples)
        auto runtime_dir = lib_path / "runtime";
        auto docs_dir = lib_path / "docs";
        auto examples_dir = lib_path / "examples";
        if (fs::exists(runtime_dir)) {
            result << "  runtime/\n";
        }
        if (fs::exists(docs_dir)) {
            result << "  docs/\n";
        }
        if (fs::exists(examples_dir)) {
            result << "  examples/\n";
        }
        result << "\n";
    }

    result << "Total: " << total_src << " source files, " << total_tests << " test files across "
           << libraries.size() << " libraries\n";

    return ToolResult::text(result.str());
}

// ============================================================================
// project/affected-tests Tool
// ============================================================================

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
                {".test-cache", build_dir / ".test-cache"},
                {"cache/meta", build_dir / "cache" / "meta"},
                {".incr-cache", build_dir / ".incr-cache"},
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
