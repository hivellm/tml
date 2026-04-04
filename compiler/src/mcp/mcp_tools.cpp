TML_MODULE("mcp")

//! # MCP Compiler Tools — Core
//!
//! Tool registration, definitions, shared helpers, and compiler tool handlers
//! (compile, check, run, build).
//!
//! ## Split Structure
//!
//! The MCP tools are split across multiple files:
//! - `mcp_tools.cpp` — This file (core registration + compile/check/run/build handlers)
//! - `mcp_tools_codegen.cpp` — emit-ir, emit-mir, test, format, lint handlers
//! - `mcp_tools_debug.cpp` — inspect, profile, debug tool handlers
//! - `mcp_tools_docs.cpp` — Documentation search infrastructure + search handler
//! - `mcp_tools_docs_handlers.cpp` — docs/get, docs/list, docs/resolve handlers
//! - `mcp_tools_project.cpp` — cache, project/build, coverage, explain, structure, etc.

#include "doc/doc_model.hpp"
#include "doc/extractor.hpp"
#include "mcp_tools_internal.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace tml::mcp {

// ============================================================================
// Docs Hint Helpers
// ============================================================================

/// Extracts candidate type/identifier names from an error string.
/// Finds single-quoted tokens and qualified module paths (containing "::").
auto extract_hint_candidates(const std::string& text) -> std::vector<std::string> {
    std::vector<std::string> hints;

    // Find single-quoted identifiers: 'Foo', 'bar', 'std::collections::List'
    std::string::size_type pos = 0;
    while ((pos = text.find('\'', pos)) != std::string::npos) {
        auto end = text.find('\'', pos + 1);
        if (end != std::string::npos && end - pos > 1 && end - pos < 60) {
            hints.push_back(text.substr(pos + 1, end - pos - 1));
        }
        pos = (end != std::string::npos) ? end + 1 : text.size();
    }

    // Find qualified module paths like "std::collections::HashMap"
    std::string::size_type mpos = 0;
    while ((mpos = text.find("::", mpos)) != std::string::npos) {
        // Walk backwards to the start of the path
        auto start = mpos;
        while (start > 0 && (std::isalnum(static_cast<unsigned char>(text[start - 1])) ||
                             text[start - 1] == '_' || text[start - 1] == ':')) {
            --start;
        }
        // Walk forwards to the end of the path
        auto end = mpos + 2;
        while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) ||
                                     text[end] == '_' || text[end] == ':')) {
            ++end;
        }
        if (end - start > 4) {
            hints.push_back(text.substr(start, end - start));
        }
        mpos = end;
    }

    // Deduplicate
    std::sort(hints.begin(), hints.end());
    hints.erase(std::unique(hints.begin(), hints.end()), hints.end());

    return hints;
}

/// Builds docs hint lines from extracted candidates (max 5 hints).
/// Returns empty string if no useful hints are found.
auto build_docs_hints(const std::string& error_text) -> std::string {
    auto candidates = extract_hint_candidates(error_text);

    std::string hints_section;

    // Known type quick-reference for common patterns
    bool mentioned_outcome = error_text.find("Outcome") != std::string::npos ||
                             error_text.find("Result") != std::string::npos;
    bool mentioned_maybe = error_text.find("Maybe") != std::string::npos ||
                           error_text.find("Option") != std::string::npos;
    bool mentioned_iterator = error_text.find("Iterator") != std::string::npos;

    if (mentioned_outcome) {
        hints_section += "  Outcome[T,E] variants: Ok(T) | Err(E)\n";
        hints_section += "  Use: docs_get(id=\"core::error::Outcome\")\n";
    }
    if (mentioned_maybe) {
        hints_section += "  Maybe[T] variants: Just(T) | Nothing\n";
        hints_section += "  Use: docs_get(id=\"core::types::option::Maybe\")\n";
    }
    if (mentioned_iterator) {
        hints_section += "  Iterator requires: type Item, func next(mut this) -> Maybe[Item]\n";
        hints_section += "  Use: docs_list(module=\"core::iter\")\n";
    }

    // Add search hints for extracted candidates (up to 5 total, skip very short names)
    int hint_count = 0;
    for (const auto& hint : candidates) {
        if (hint_count >= 5)
            break;
        if (hint.size() < 3 || hint.size() >= 60)
            continue;
        hints_section += "  Try: docs_search(query=\"" + hint + "\")\n";
        ++hint_count;
    }

    if (hints_section.empty()) {
        return {};
    }
    return "\n\n--- Docs Hints ---\n" + hints_section;
}

/// Parses a TML source file for top-level `use` statements and returns
/// the module prefix of each import (e.g. "std::collections" from
/// "use std::collections::List").
auto extract_imports(const std::string& file_path) -> std::vector<std::string> {
    FILE* fp = fopen(file_path.c_str(), "r");
    if (!fp) {
        return {};
    }

    std::vector<std::string> imports;
    char line_buf[1024];
    while (fgets(line_buf, sizeof(line_buf), fp) != nullptr) {
        std::string src_line(line_buf);
        auto use_pos = src_line.find("use ");
        if (use_pos == std::string::npos)
            continue;

        // Accept lines that start with "use " (allowing leading whitespace)
        bool leading_ok = true;
        for (std::string::size_type i = 0; i < use_pos; ++i) {
            if (src_line[i] != ' ' && src_line[i] != '\t') {
                leading_ok = false;
                break;
            }
        }
        if (!leading_ok)
            continue;

        // Extract the module path after "use "
        auto mod_start = use_pos + 4;
        // Skip leading whitespace
        while (mod_start < src_line.size() && src_line[mod_start] == ' ')
            ++mod_start;

        // Collect up to the first "{" or end-of-line (skip trailing semicolons)
        auto mod_end = src_line.find("::{", mod_start);
        if (mod_end == std::string::npos)
            mod_end = src_line.size();

        std::string mod = src_line.substr(mod_start, mod_end - mod_start);

        // Trim trailing whitespace, semicolons, and newline characters
        while (!mod.empty() && (mod.back() == ' ' || mod.back() == '\t' || mod.back() == '\n' ||
                                mod.back() == '\r' || mod.back() == ';')) {
            mod.pop_back();
        }

        if (!mod.empty() && mod != "test") {
            imports.push_back(mod);
        }
    }

    fclose(fp);

    // Deduplicate
    std::sort(imports.begin(), imports.end());
    imports.erase(std::unique(imports.begin(), imports.end()), imports.end());

    return imports;
}

// ============================================================================
// Tool Registration
// ============================================================================

void register_compiler_tools(McpServer& server) {
    server.register_tool(make_compile_tool(), handle_compile);
    server.register_tool(make_run_tool(), handle_run);
    server.register_tool(make_build_tool(), handle_build);
    server.register_tool(make_check_tool(), handle_check);
    server.register_tool(make_emit_ir_tool(), handle_emit_ir);
    server.register_tool(make_emit_mir_tool(), handle_emit_mir);
    server.register_tool(make_test_tool(), handle_test);
    server.register_tool(make_format_tool(), handle_format);
    server.register_tool(make_lint_tool(), handle_lint);
    server.register_tool(make_docs_search_tool(), handle_docs_search);
    server.register_tool(make_docs_get_tool(), handle_docs_get);
    server.register_tool(make_docs_list_tool(), handle_docs_list);
    server.register_tool(make_docs_resolve_tool(), handle_docs_resolve);
    server.register_tool(make_cache_invalidate_tool(), handle_cache_invalidate);
    server.register_tool(make_project_build_tool(), handle_project_build);
    server.register_tool(make_project_coverage_tool(), handle_project_coverage);
    server.register_tool(make_explain_tool(), handle_explain);
    server.register_tool(make_project_structure_tool(), handle_project_structure);
    server.register_tool(make_project_affected_tests_tool(), handle_project_affected_tests);
    server.register_tool(make_project_artifacts_tool(), handle_project_artifacts);
    server.register_tool(make_project_slow_tests_tool(), handle_project_slow_tests);
    server.register_tool(make_inspect_tool(), handle_inspect);
    server.register_tool(make_profile_tool(), handle_profile);
    server.register_tool(make_debug_tool(), handle_debug);
    server.register_tool(make_analyze_tool(), handle_analyze);
}

// ============================================================================
// Tool Definitions
// ============================================================================

auto make_compile_tool() -> Tool {
    return Tool{.name = "compile",
                .description = "Compile a TML source file to executable or library",
                .parameters = {
                    {"file", "string", "Path to the source file", true},
                    {"output", "string", "Output file path", false},
                    {"optimize", "string", "Optimization level (O0, O1, O2, O3)", false},
                    {"release", "boolean", "Build in release mode with optimizations", false},
                }};
}

auto make_check_tool() -> Tool {
    return Tool{.name = "check",
                .description = "Type check a TML source file without compiling",
                .parameters = {
                    {"file", "string", "Path to the source file", true},
                }};
}

auto make_run_tool() -> Tool {
    return Tool{.name = "run",
                .description = "Build and execute a TML source file, returning program output",
                .parameters = {
                    {"file", "string", "Path to the source file", true},
                    {"args", "array", "Arguments to pass to the program", false},
                    {"release", "boolean", "Build in release mode with optimizations", false},
                }};
}

auto make_build_tool() -> Tool {
    return Tool{.name = "build",
                .description = "Build a TML source file to executable with full options",
                .parameters = {
                    {"file", "string", "Path to the source file", true},
                    {"output", "string", "Output file path", false},
                    {"optimize", "string", "Optimization level (O0, O1, O2, O3)", false},
                    {"release", "boolean", "Build in release mode with optimizations", false},
                    {"crate_type", "string", "Output type: bin, lib, dylib, rlib", false},
                }};
}

auto make_emit_ir_tool() -> Tool {
    return Tool{
        .name = "emit-ir",
        .description =
            "Emit LLVM IR for a TML source file. Supports chunked output to avoid token limits.",
        .parameters = {
            {"file", "string", "Path to the source file", true},
            {"optimize", "string", "Optimization level (O0, O1, O2, O3)", false},
            {"function", "string", "Filter output to a specific function name", false},
            {"offset", "number", "Line offset for chunked output (0-based)", false},
            {"limit", "number", "Maximum number of lines to return", false},
        }};
}

auto make_emit_mir_tool() -> Tool {
    return Tool{.name = "emit-mir",
                .description = "Emit MIR (Mid-level IR) for a TML source file",
                .parameters = {
                    {"file", "string", "Path to the source file", true},
                }};
}

auto make_test_tool() -> Tool {
    return Tool{.name = "test",
                .description = "Run TML tests",
                .parameters = {
                    {"path", "string", "Path to test file or directory", false},
                    {"filter", "string", "Test name filter", false},
                    {"suite", "string",
                     "Run only tests in a specific suite group. Examples: \"core/str\", "
                     "\"std/json\", \"core/fmt\", \"std/collections\", \"compiler/compiler\"",
                     false},
                    {"release", "boolean", "Run in release mode", false},
                    {"coverage", "boolean", "Generate coverage report", false},
                    {"profile", "boolean", "Show per-test timing profile", false},
                    {"verbose", "boolean", "Show verbose output", false},
                    {"no_cache", "boolean", "Force full recompilation (disable test cache)", false},
                    {"fail_fast", "boolean", "Stop on first test failure", false},
                    {"structured", "boolean",
                     "Return parsed results: total, passed, failed, failures[], timeouts[]", false},
                    {"debug_layers", "boolean",
                     "Emit multi-layer IR diagnostics on failure (default: true). "
                     "Set to false to disable. Includes HIR + MIR + LLVM IR for failing functions.",
                     false},
                }};
}

auto make_format_tool() -> Tool {
    return Tool{.name = "format",
                .description = "Format TML source files",
                .parameters = {
                    {"file", "string", "Path to the source file or directory", true},
                    {"check", "boolean", "Check formatting without modifying files", false},
                }};
}

auto make_lint_tool() -> Tool {
    return Tool{.name = "lint",
                .description = "Lint TML source files for style and potential issues",
                .parameters = {
                    {"file", "string", "Path to the source file or directory", true},
                    {"fix", "boolean", "Automatically fix issues where possible", false},
                }};
}

auto make_docs_search_tool() -> Tool {
    return Tool{
        .name = "docs/search",
        .description = "Search TML documentation",
        .parameters = {
            {"query", "string", "Search query", true},
            {"limit", "number", "Maximum results (default: 10)", false},
            {"kind", "string",
             "Filter by item kind: function, method, struct, enum, behavior, constant", false},
            {"module", "string", "Filter by module path (e.g. core::str, std::json)", false},
            {"mode", "string",
             "Search mode: text (BM25), semantic (HNSW vector), hybrid (both, default)", false},
        }};
}

// ============================================================================
// Shared Helper: Read File
// ============================================================================

auto read_source_file(const std::string& path) -> std::optional<std::string> {
#ifdef _WIN32
    // std::ifstream::rdbuf() can block on Windows in MCP server context (inherited handles).
    // Use Win32 CreateFile/ReadFile instead.
    HANDLE hf = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    LARGE_INTEGER file_size{};
    GetFileSizeEx(hf, &file_size);
    std::string content;
    content.resize(static_cast<size_t>(file_size.QuadPart));
    DWORD bytes_read = 0;
    ReadFile(hf, content.data(), static_cast<DWORD>(file_size.QuadPart), &bytes_read, nullptr);
    content.resize(bytes_read);
    CloseHandle(hf);
    return content;
#else
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
#endif
}

// ============================================================================
// Shared Helper: Strip ANSI Escape Codes
// ============================================================================

auto strip_ansi(const std::string& input) -> std::string {
    std::string out;
    out.reserve(input.size());
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '\033' && i + 1 < input.size() && input[i + 1] == '[') {
            // Skip ANSI escape sequence: ESC [ ... final_byte
            i += 2;
            while (i < input.size() && input[i] >= 0x20 && input[i] <= 0x3F) {
                ++i; // parameter bytes
            }
            while (i < input.size() && input[i] >= 0x20 && input[i] <= 0x2F) {
                ++i; // intermediate bytes
            }
            if (i < input.size()) {
                ++i; // final byte
            }
        } else {
            out += input[i];
            ++i;
        }
    }
    return out;
}

// ============================================================================
// Shared Helper: Execute Command and Capture Output
// ============================================================================

auto execute_command(const std::string& cmd, int timeout_seconds) -> std::pair<std::string, int> {
    // Reject shell injection: pipe, grep, redirect, command chaining.
    // The MCP tools must NEVER pipe test output through grep/filter.
    // Use structured output mode instead.
    static const char* forbidden[] = {"|", "grep", ">>", "&&", ";", "`", "$("};
    for (const auto& token : forbidden) {
        if (cmd.find(token) != std::string::npos) {
            return {"[BLOCKED] Shell operators are forbidden in MCP commands. "
                    "Found '" +
                        std::string(token) +
                        "' in command. "
                        "Use structured output or MCP tool parameters instead of shell piping.",
                    1};
        }
    }

    std::string output;
    int exit_code = -1;

    auto start_time = std::chrono::steady_clock::now();

#ifdef _WIN32
    // Windows: Use CreateProcess with explicit NUL stdin to prevent the child
    // from inheriting the parent's stdin pipe (MCP protocol), which causes hangs.
    {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        // Create pipe for reading child's stdout+stderr
        HANDLE read_pipe = nullptr;
        HANDLE write_pipe = nullptr;
        if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
            return {"Failed to create pipe", -1};
        }
        SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

        // Open NUL for child's stdin
        HANDLE nul_handle = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        // Use PROC_THREAD_ATTRIBUTE_LIST to inherit ONLY the specific handles the
        // child needs (write_pipe, nul_handle), not ALL inheritable handles.  Without
        // this, CreateProcess with bInheritHandle=TRUE passes the parent's stdout pipe
        // to the child, keeping it open and preventing MCP responses from flushing.
        SIZE_T attr_size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
        auto attr_buf = std::make_unique<char[]>(attr_size);
        auto* attr_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.get());
        InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_size);

        HANDLE inherit_handles[2] = {write_pipe, nul_handle};
        int num_inherit = (nul_handle != INVALID_HANDLE_VALUE) ? 2 : 1;
        UpdateProcThreadAttribute(attr_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit_handles,
                                  num_inherit * sizeof(HANDLE), nullptr, nullptr);

        STARTUPINFOEXA siex{};
        siex.StartupInfo.cb = sizeof(siex);
        siex.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        siex.StartupInfo.hStdInput = nul_handle;
        siex.StartupInfo.hStdOutput = write_pipe;
        siex.StartupInfo.hStdError = write_pipe;
        siex.lpAttributeList = attr_list;

        PROCESS_INFORMATION pi{};
        std::string full_cmd = cmd;

        std::cerr << "[exec] CreateProcess: " << full_cmd << std::endl;
        BOOL ok = CreateProcessA(nullptr, full_cmd.data(), nullptr, nullptr, TRUE,
                                 CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                 &siex.StartupInfo, &pi);

        DeleteProcThreadAttributeList(attr_list);

        CloseHandle(write_pipe); // Close write end in parent
        if (nul_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(nul_handle);
        }

        if (!ok) {
            std::cerr << "[exec] CreateProcess FAILED" << std::endl;
            CloseHandle(read_pipe);
            return {"Failed to create process: " + cmd, -1};
        }
        std::cerr << "[exec] Process created, PID=" << pi.dwProcessId << std::endl;

        // Read output from child
        char buffer[4096];
        DWORD bytes_read = 0;
        std::cerr << "[exec] Reading pipe..." << std::endl;
        while (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) &&
               bytes_read > 0) {
            buffer[bytes_read] = '\0';
            output += buffer;

            // Check timeout
            if (timeout_seconds > 0) {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                if (elapsed_s >= timeout_seconds) {
                    output += "\n[TIMEOUT] Command exceeded " + std::to_string(timeout_seconds) +
                              "s limit.\n";
                    TerminateProcess(pi.hProcess, 124);
                    CloseHandle(read_pipe);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    return {strip_ansi(output), 124};
                }
            }
        }

        std::cerr << "[exec] Pipe reading done, closing pipe" << std::endl;
        CloseHandle(read_pipe);
        std::cerr << "[exec] Waiting for process..." << std::endl;
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD win_exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &win_exit_code);
        exit_code = static_cast<int>(win_exit_code);
        std::cerr << "[exec] Process exited with code " << exit_code << std::endl;

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#else
    // Unix: Use popen/pclose
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (pipe) {
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;

            // Check timeout
            if (timeout_seconds > 0) {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                if (elapsed_s >= timeout_seconds) {
                    output += "\n[TIMEOUT] Command exceeded " + std::to_string(timeout_seconds) +
                              "s limit.\n";
                    pclose(pipe);
                    return {strip_ansi(output), 124};
                }
            }
        }
        int status = pclose(pipe);
        exit_code = WEXITSTATUS(status);
    }
#endif

    return {strip_ansi(output), exit_code};
}

// ============================================================================
// Shared Helper: Get TML Executable
// ============================================================================

auto get_tml_executable() -> std::string {
    // Try to find tml.exe relative to the current executable or in PATH
#ifdef _WIN32
    std::vector<std::string> paths = {
        "tml.exe",
        "./build/debug/bin/tml.exe",
        "./build/debug/tml.exe",
        "./build/release/bin/tml.exe",
        "./build/release/tml.exe",
    };
    for (const auto& path : paths) {
        if (fs::exists(path)) {
            return fs::absolute(path).string();
        }
    }
    return "tml.exe";
#else
    return "tml";
#endif
}

// ============================================================================
// Tool Handlers
// ============================================================================

auto handle_compile(const json::JsonValue& params) -> ToolResult {
    // Get file parameter
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    // Check file exists
    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    // Build command - use the TML executable for full compilation
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " build " << file_path;

    // Add output if specified
    auto* output_param = params.get("output");
    if (output_param != nullptr && output_param->is_string()) {
        cmd << " -o " << output_param->as_string();
    }

    // Add optimization level if specified
    auto* optimize_param = params.get("optimize");
    if (optimize_param != nullptr && optimize_param->is_string()) {
        std::string opt = optimize_param->as_string();
        if (opt == "O0" || opt == "O1" || opt == "O2" || opt == "O3") {
            cmd << " -" << opt;
        }
    }

    // Add release flag if specified
    auto* release_param = params.get("release");
    if (release_param != nullptr && release_param->is_bool() && release_param->as_bool()) {
        cmd << " --release";
    }

    // Execute compilation
    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Compilation successful!\n";
        result << "File: " << file_path << "\n";

        // Determine output file name
        std::string output_file;
        if (output_param != nullptr && output_param->is_string()) {
            output_file = output_param->as_string();
        } else {
            // Default output is input file stem + .exe (Windows) or no extension (Unix)
            fs::path input_path(file_path);
#ifdef _WIN32
            output_file = (input_path.parent_path() / input_path.stem()).string() + ".exe";
#else
            output_file = (input_path.parent_path() / input_path.stem()).string();
#endif
        }
        result << "Output: " << output_file << "\n";
    } else {
        result << "Compilation failed (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Compiler Output ---\n" << output;
    }

    if (exit_code != 0) {
        return ToolResult::error(result.str());
    }

    return ToolResult::text(result.str());
}

auto handle_check(const json::JsonValue& params) -> ToolResult {
    // Get file parameter
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    // Check file exists
    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    // Use subprocess execution — the in-process parse_and_check lacks full
    // module resolution context (query system, import paths, std/core libs)
    // which causes crashes. The tml.exe check command handles all of this.
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " check " << file_path;

    auto [output, exit_code] = execute_command(cmd.str());

    // Strip ANSI escape codes from compiler output
    std::string clean_output = strip_ansi(output);

    if (exit_code == 0) {
        std::string result = "Type check passed for: " + file_path;

        // Show imported modules with docs_list suggestions
        {
            auto imports = extract_imports(file_path);
            if (!imports.empty()) {
                result += "\n\n--- Imported Modules ---";
                for (const auto& imp : imports) {
                    result += "\n  " + imp + " -> docs_list(module=\"" + imp + "\")";
                }
            }
        }

        if (!clean_output.empty()) {
            result += "\n\n" + clean_output;
        }
        return ToolResult::text(result);
    }

    std::string error_msg = "Type check failed for: " + file_path;
    if (!clean_output.empty()) {
        error_msg += "\n\n" + clean_output;
    }

    error_msg += build_docs_hints(clean_output);

    return ToolResult::error(error_msg);
}

auto handle_run(const json::JsonValue& params) -> ToolResult {
    // Get file parameter
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    // Check file exists
    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    // Build command
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " run " << file_path;

    // Add release flag if specified
    auto* release_param = params.get("release");
    if (release_param != nullptr && release_param->is_bool() && release_param->as_bool()) {
        cmd << " --release";
    }

    // Add program arguments if specified
    auto* args_param = params.get("args");
    if (args_param != nullptr && args_param->is_array()) {
        for (const auto& arg : args_param->as_array()) {
            if (arg.is_string()) {
                cmd << " " << arg.as_string();
            }
        }
    }

    // Execute
    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    result << "Exit code: " << exit_code << "\n";
    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    if (exit_code != 0) {
        return ToolResult::error(result.str());
    }

    return ToolResult::text(result.str());
}

auto handle_build(const json::JsonValue& params) -> ToolResult {
    // Get file parameter
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    // Check file exists
    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    // Build command
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " build " << file_path;

    // Add output if specified
    auto* output_param = params.get("output");
    if (output_param != nullptr && output_param->is_string()) {
        cmd << " -o " << output_param->as_string();
    }

    // Add optimization level if specified
    auto* optimize_param = params.get("optimize");
    if (optimize_param != nullptr && optimize_param->is_string()) {
        std::string opt = optimize_param->as_string();
        if (opt == "O0" || opt == "O1" || opt == "O2" || opt == "O3") {
            cmd << " -" << opt;
        }
    }

    // Add release flag if specified
    auto* release_param = params.get("release");
    if (release_param != nullptr && release_param->is_bool() && release_param->as_bool()) {
        cmd << " --release";
    }

    // Add crate type if specified
    auto* crate_type_param = params.get("crate_type");
    if (crate_type_param != nullptr && crate_type_param->is_string()) {
        cmd << " --crate-type=" << crate_type_param->as_string();
    }

    // Execute
    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Build successful!\n";
        result << "File: " << file_path << "\n";
    } else {
        result << "Build failed (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    if (exit_code != 0) {
        return ToolResult::error(result.str());
    }

    return ToolResult::text(result.str());
}

// Codegen handlers (handle_emit_ir, handle_emit_mir, handle_test, handle_format, handle_lint)
// are defined in mcp_tools_codegen.cpp.

// Debug/inspector handlers (handle_inspect, handle_profile, handle_debug)
// and their tool definitions (make_inspect_tool, make_profile_tool, make_debug_tool)
// are defined in mcp_tools_debug.cpp.

} // namespace tml::mcp
