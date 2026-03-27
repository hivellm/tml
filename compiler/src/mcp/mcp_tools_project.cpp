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
                    {"tests", "boolean", "Build C++ test executable (default: false)", false},
                    {"target", "string",
                     "Build target: \"compiler\" (default, tml.exe only), \"all\", "
                     "\"mcp\" (tml_mcp.exe only). Defaults to \"compiler\" to avoid "
                     "rebuilding the running MCP server.",
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

    bool build_tests = false;
    auto* tests_param = params.get("tests");
    if (tests_param != nullptr && tests_param->is_bool()) {
        build_tests = tests_param->as_bool();
    }

    // Parse target: "compiler" (default, tml.exe only), "all", "mcp" (tml_mcp.exe only)
    // Default to "compiler" to avoid rebuilding the running MCP server (tml_mcp.exe),
    // which would kill the active connection.
    std::string target = "compiler";
    auto* target_param = params.get("target");
    if (target_param != nullptr && target_param->is_string()) {
        target = target_param->as_string();
        if (target != "all" && target != "compiler" && target != "mcp") {
            return ToolResult::error(
                "Invalid target: \"" + target +
                "\". Use \"compiler\" (tml.exe, default), \"all\", or \"mcp\" (tml_mcp.exe).");
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
    if (build_tests) {
        cmd << " --tests";
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
    if (build_tests) {
        cmd << " --tests";
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

} // namespace tml::mcp
