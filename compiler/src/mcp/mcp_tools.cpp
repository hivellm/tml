TML_MODULE("mcp")

//! # MCP Compiler Tools — Core
//!
//! Tool registration, definitions, shared helpers, and compiler tool handlers
//! (compile, check, run, build, emit-ir, emit-mir, test, format, lint).
//!
//! ## Split Structure
//!
//! The MCP tools are split across multiple files:
//! - `mcp_tools.cpp` — This file (core registration + compiler handlers)
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
static auto extract_hint_candidates(const std::string& text) -> std::vector<std::string> {
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
static auto build_docs_hints(const std::string& error_text) -> std::string {
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
static auto extract_imports(const std::string& file_path) -> std::vector<std::string> {
    std::ifstream source(file_path);
    if (!source.is_open()) {
        return {};
    }

    std::vector<std::string> imports;
    std::string src_line;
    while (std::getline(source, src_line)) {
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

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = nul_handle;
        si.hStdOutput = write_pipe;
        si.hStdError = write_pipe; // redirect stderr to stdout

        PROCESS_INFORMATION pi{};
        std::string full_cmd = cmd;

        std::cerr << "[exec] CreateProcess: " << full_cmd << std::endl;
        BOOL ok = CreateProcessA(nullptr, full_cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                 nullptr, nullptr, &si, &pi);

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

        // Show imported modules with docs_list suggestions so the LLM can
        // quickly discover what APIs are available in the modules it is using.
        auto imports = extract_imports(file_path);
        if (!imports.empty()) {
            result += "\n\n--- Imported Modules ---";
            for (const auto& imp : imports) {
                result += "\n  " + imp + " -> docs_list(module=\"" + imp + "\")";
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

    // Append docs hints so the LLM can look up types that appear in the error.
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

auto handle_emit_ir(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    // Use subprocess execution — the in-process parse_and_check + codegen lacks
    // full module resolution context, causing crashes on any file with imports.
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " build " << file_path << " --emit-ir";

    // Add optimization level if specified
    auto* optimize_param = params.get("optimize");
    if (optimize_param != nullptr && optimize_param->is_string()) {
        std::string opt = optimize_param->as_string();
        if (opt == "O0" || opt == "O1" || opt == "O2" || opt == "O3") {
            cmd << " -" << opt;
        }
    }

    auto [output, exit_code] = execute_command(cmd.str());

    if (exit_code != 0) {
        return ToolResult::error("emit-ir failed (exit code " + std::to_string(exit_code) +
                                 ")\n\n" + strip_ansi(output));
    }

    // The CLI writes IR to build/debug/<module_name>.ll — read it back
    std::string module_name = fs::path(file_path).stem().string();
    std::string ll_path_str = "build/debug/" + module_name + ".ll";

    auto ir_opt = read_source_file(ll_path_str);
    std::string ir = ir_opt.has_value() ? *ir_opt : "(could not read " + ll_path_str + ")";

    if (ir.size() > 500000) {
        ir = ir.substr(0, 500000) + "\n\n[... truncated at 500KB ...]";
    }

    // Apply function filter if specified
    auto* func_param = params.get("function");
    if (func_param != nullptr && func_param->is_string()) {
        std::string func_name = func_param->as_string();
        std::stringstream filtered;
        std::istringstream stream(ir);
        std::string line;
        bool in_function = false;
        int brace_depth = 0;

        while (std::getline(stream, line)) {
            if (!in_function) {
                if (line.find("define") != std::string::npos &&
                    line.find(func_name) != std::string::npos) {
                    in_function = true;
                    brace_depth = 0;
                    filtered << line << "\n";
                    for (char c : line) {
                        if (c == '{')
                            brace_depth++;
                        if (c == '}')
                            brace_depth--;
                    }
                }
            } else {
                filtered << line << "\n";
                for (char c : line) {
                    if (c == '{')
                        brace_depth++;
                    if (c == '}')
                        brace_depth--;
                }
                if (brace_depth <= 0) {
                    in_function = false;
                    filtered << "\n";
                }
            }
        }

        ir = filtered.str();
        if (ir.empty()) {
            ir = "Function '" + func_name + "' not found in IR output.\n";
        }
    }

    // Apply offset/limit for chunked output
    auto* offset_param = params.get("offset");
    auto* limit_param = params.get("limit");

    if ((offset_param != nullptr && offset_param->is_number()) ||
        (limit_param != nullptr && limit_param->is_number())) {
        int64_t offset = 0;
        int64_t limit = -1;
        if (offset_param != nullptr && offset_param->is_number()) {
            offset = offset_param->as_i64();
        }
        if (limit_param != nullptr && limit_param->is_number()) {
            limit = limit_param->as_i64();
        }

        std::istringstream stream(ir);
        std::string line;
        std::stringstream chunked;
        int64_t line_num = 0;
        int64_t total_lines = 0;

        // Count total lines first
        {
            std::istringstream counter(ir);
            std::string l;
            while (std::getline(counter, l))
                total_lines++;
        }

        while (std::getline(stream, line)) {
            if (line_num >= offset && (limit < 0 || line_num < offset + limit)) {
                chunked << line << "\n";
            }
            line_num++;
        }

        ir = chunked.str();

        // Add metadata
        std::stringstream meta;
        meta << "Lines: " << offset << "-"
             << std::min(offset + (limit > 0 ? limit : total_lines), total_lines) << " of "
             << total_lines << "\n\n";
        ir = meta.str() + ir;
    }

    return ToolResult::text(ir);
}

auto handle_emit_mir(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    // Use subprocess execution — the in-process approach lacks full module
    // resolution context, causing crashes on any file with imports.
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " build " << file_path << " --emit-mir";

    auto [output, exit_code] = execute_command(cmd.str());

    if (exit_code != 0) {
        return ToolResult::error("emit-mir failed (exit code " + std::to_string(exit_code) +
                                 ")\n\n" + strip_ansi(output));
    }

    // The CLI writes MIR to build/debug/<module_name>.mir — read it back
    std::string module_name = fs::path(file_path).stem().string();
    fs::path mir_path = fs::path("build") / "debug" / (module_name + ".mir");
    auto mir_opt = read_source_file(mir_path.string());
    if (!mir_opt.has_value()) {
        return ToolResult::error("emit-mir succeeded but could not read output file: " +
                                 mir_path.string());
    }

    std::string mir = *mir_opt;
    if (mir.size() > 500000) {
        mir = mir.substr(0, 500000) + "\n\n[... truncated at 500KB ...]";
    }
    return ToolResult::text(mir);
}

auto handle_test(const json::JsonValue& params) -> ToolResult {
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " test";

    // Add path if specified
    auto* path_param = params.get("path");
    if (path_param != nullptr && path_param->is_string()) {
        cmd << " " << path_param->as_string();
    }

    // Add filter if specified (maps to --filter=X for file path substring matching)
    auto* filter_param = params.get("filter");
    if (filter_param != nullptr && filter_param->is_string()) {
        cmd << " --filter=" << filter_param->as_string();
    }

    // Add suite filter (maps to --suite=X for suite group filtering)
    // e.g., suite="core/str" runs only str tests from lib/core
    auto* suite_param = params.get("suite");
    if (suite_param != nullptr && suite_param->is_string()) {
        cmd << " --suite=" << suite_param->as_string();
    }

    // Add release flag
    auto* release_param = params.get("release");
    if (release_param != nullptr && release_param->is_bool() && release_param->as_bool()) {
        cmd << " --release";
    }

    // Add coverage flag
    auto* coverage_param = params.get("coverage");
    if (coverage_param != nullptr && coverage_param->is_bool() && coverage_param->as_bool()) {
        cmd << " --coverage";
    }

    // Add profile flag
    auto* profile_param = params.get("profile");
    if (profile_param != nullptr && profile_param->is_bool() && profile_param->as_bool()) {
        cmd << " --profile";
    }

    // Always add --verbose: without it, tml test produces no stdout/stderr output
    // (all output is INFO-level log messages that only appear with --verbose).
    // The MCP test tool needs parseable output for both structured and text modes.
    cmd << " --verbose";

    // Add no-cache flag
    auto* no_cache_param = params.get("no_cache");
    if (no_cache_param != nullptr && no_cache_param->is_bool() && no_cache_param->as_bool()) {
        cmd << " --no-cache";
    }

    // Add fail-fast / no-fail-fast flag
    auto* fail_fast_param = params.get("fail_fast");
    if (fail_fast_param != nullptr && fail_fast_param->is_bool()) {
        if (fail_fast_param->as_bool()) {
            cmd << " --fail-fast";
        } else {
            cmd << " --no-fail-fast";
        }
    }

    // debug-layers: enabled by default (Condition B for LLM debugging research).
    // Can be explicitly disabled via debug_layers=false or TML_DEBUG_LAYERS=0.
    // When enabled, test failures include multi-layer IR diagnostics (HIR + MIR + LLVM IR).
    auto* debug_layers_param = params.get("debug_layers");
    bool debug_layers = true; // Default ON for Condition B
    if (debug_layers_param != nullptr && debug_layers_param->is_bool()) {
        debug_layers = debug_layers_param->as_bool();
    } else {
        const char* env_val = std::getenv("TML_DEBUG_LAYERS");
        if (env_val != nullptr && std::string(env_val) == "0") {
            debug_layers = false;
        }
    }
    if (debug_layers) {
        cmd << " --debug-layers";
    }

    // Timeout: 300s for normal tests, 600s for coverage/full suite
    auto* cov_check = params.get("coverage");
    bool is_full_suite = (path_param == nullptr || !path_param->is_string()) &&
                         (suite_param == nullptr || !suite_param->is_string()) &&
                         (filter_param == nullptr || !filter_param->is_string());
    int test_timeout = (cov_check && cov_check->is_bool() && cov_check->as_bool()) ? 600
                       : is_full_suite                                             ? 600
                                                                                   : 300;
    auto [output, exit_code] = execute_command(cmd.str(), test_timeout);

    // Check if structured output requested
    auto* structured_param = params.get("structured");
    if (structured_param != nullptr && structured_param->is_bool() && structured_param->as_bool()) {
        // Parse test output for structured results
        std::stringstream result;
        result << "{";

        // Parse v3 coordinator output format:
        //   "  Tests:   N" / "  Passed:  N" / "  Failed:  N"
        //   "  Crashed: N" / "  Compile errors: N"
        //   "FAIL group/name (file) [exit N]"
        //   "COMPILE ERROR suite: message"
        int total = 0, passed = 0, failed = 0, crashed = 0, compile_errors = 0;
        std::vector<std::string> failures;
        std::vector<std::string> timeouts;
        std::vector<std::string> diagnostics; // full error messages with file:line:col

        auto extract_number = [](const std::string& l, const std::string& key) -> int {
            auto pos = l.find(key);
            if (pos == std::string::npos)
                return -1;
            auto val_start = pos + key.size();
            while (val_start < l.size() && l[val_start] == ' ')
                ++val_start;
            try {
                return std::stoi(l.substr(val_start));
            } catch (...) {
                return -1;
            }
        };

        // Strip ANSI escape codes (\033[...m) so diagnostics are clean text
        auto strip_ansi = [](const std::string& s) -> std::string {
            std::string r;
            r.reserve(s.size());
            for (size_t i = 0; i < s.size();) {
                if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
                    i += 2;
                    while (i < s.size() && s[i] != 'm')
                        ++i;
                    if (i < s.size())
                        ++i;
                } else {
                    r += s[i++];
                }
            }
            return r;
        };

        // Escape a string for JSON embedding
        auto json_esc = [](const std::string& s) -> std::string {
            std::string r;
            for (char c : s) {
                if (c == '"')
                    r += "\\\"";
                else if (c == '\\')
                    r += "\\\\";
                else if (c == '\n')
                    r += "\\n";
                else if (c == '\r') {
                } else if (c == '\t')
                    r += "\\t";
                else
                    r += c;
            }
            return r;
        };

        std::istringstream stream(output);
        std::string line;
        bool in_fail_detail = false; // collecting indented error lines after a test FAIL
        while (std::getline(stream, line)) {
            std::string clean = strip_ansi(line);

            int v;
            if ((v = extract_number(clean, "Passed:")) >= 0)
                passed = v;
            if ((v = extract_number(clean, "Failed:")) >= 0)
                failed = v;
            if ((v = extract_number(clean, "Tests:")) >= 0)
                total = v;
            if ((v = extract_number(clean, "Crashed:")) >= 0)
                crashed = v;
            if ((v = extract_number(clean, "Compile errors:")) >= 0)
                compile_errors = v;

            // ── Diagnostics: capture errors with file:line:col detail ──────
            // [compile] SKIP file:line:col: error — per-file compile error
            if (clean.find("[compile] SKIP ") != std::string::npos) {
                auto pos = clean.find("[compile] SKIP ");
                diagnostics.push_back(clean.substr(pos + 15));
                in_fail_detail = false;
            }
            // [compile] FAIL suite.exe: error (N/M) — suite-level compile failure
            else if (clean.find("[compile] FAIL ") != std::string::npos) {
                auto pos = clean.find("[compile] FAIL ");
                std::string d = clean.substr(pos + 15);
                auto paren = d.rfind(" (");
                if (paren != std::string::npos)
                    d = d.substr(0, paren);
                if (!d.empty())
                    diagnostics.push_back(d);
                in_fail_detail = false;
            }
            // COMPILE ERROR suite: error (from reporter — failed suites)
            else if (clean.find("COMPILE ERROR") != std::string::npos) {
                auto pos = clean.find("COMPILE ERROR");
                diagnostics.push_back(clean.substr(pos));
                in_fail_detail = false;
            }
            // FAIL group/name (file) [exit N] — test failure header
            else if (clean.find("FAIL ") != std::string::npos &&
                     clean.find("[compile]") == std::string::npos &&
                     clean.find("COMPILE") == std::string::npos) {
                auto fail_pos = clean.find("FAIL ");
                diagnostics.push_back(clean.substr(fail_pos));
                in_fail_detail = true;
            }
            // Indented error detail following a test FAIL line
            else if (in_fail_detail) {
                // Skip log prefix "HH:MM:SS.mmm LEVEL [module] " to reach message content
                auto bracket = clean.find("] ");
                std::string msg =
                    (bracket != std::string::npos) ? clean.substr(bracket + 2) : clean;
                if (!msg.empty() && msg[0] == ' ') {
                    auto first = msg.find_first_not_of(' ');
                    if (first != std::string::npos)
                        diagnostics.push_back("  " + msg.substr(first));
                } else {
                    in_fail_detail = false;
                }
            } else {
                in_fail_detail = false;
            }
            // ─────────────────────────────────────────────────────────────

            // Collect failure names (backwards-compat field)
            if (clean.find("FAIL") != std::string::npos &&
                clean.find("COMPILE") == std::string::npos) {
                auto fail_pos = clean.find("FAIL");
                if (fail_pos != std::string::npos) {
                    auto name_start = clean.find_first_not_of(' ', fail_pos + 4);
                    if (name_start != std::string::npos) {
                        auto paren_pos = clean.find(" (", name_start);
                        auto end_pos = (paren_pos != std::string::npos) ? paren_pos : clean.size();
                        failures.push_back(clean.substr(name_start, end_pos - name_start));
                    }
                }
            }
            // Collect timeout details
            if (clean.find("TIMEOUT:") != std::string::npos) {
                auto timeout_pos = clean.find("TIMEOUT:");
                auto detail_start = clean.find_first_not_of(' ', timeout_pos);
                if (detail_start != std::string::npos)
                    timeouts.push_back(clean.substr(detail_start));
            }
            // Compile errors also added to failures list
            if (clean.find("COMPILE ERROR") != std::string::npos) {
                auto colon_pos = clean.find("COMPILE ERROR");
                if (colon_pos != std::string::npos) {
                    auto name_start = clean.find_first_not_of(' ', colon_pos + 13);
                    if (name_start != std::string::npos)
                        failures.push_back("[compile] " + clean.substr(name_start));
                }
            }
        }

        // If total wasn't explicitly set, derive from passed+failed
        if (total == 0 && (passed > 0 || failed > 0))
            total = passed + failed + crashed;

        int skipped = total - passed - failed - crashed;
        if (skipped < 0)
            skipped = 0;

        result << "\"total\":" << total << ",";
        result << "\"passed\":" << passed << ",";
        result << "\"failed\":" << failed << ",";
        result << "\"crashed\":" << crashed << ",";
        result << "\"skipped\":" << skipped << ",";
        result << "\"compile_errors\":" << compile_errors << ",";
        result << "\"failures\":[";
        for (size_t i = 0; i < failures.size(); ++i) {
            if (i > 0)
                result << ",";
            result << "\"" << json_esc(failures[i]) << "\"";
        }
        result << "],";
        result << "\"timeouts\":[";
        for (size_t i = 0; i < timeouts.size(); ++i) {
            if (i > 0)
                result << ",";
            result << "\"" << json_esc(timeouts[i]) << "\"";
        }
        result << "],";
        result << "\"diagnostics\":[";
        for (size_t i = 0; i < diagnostics.size(); ++i) {
            if (i > 0)
                result << ",";
            result << "\"" << json_esc(diagnostics[i]) << "\"";
        }
        result << "],";

        // Build docs hints from the raw output so the LLM can look up types
        // mentioned in compiler diagnostics without reading source files.
        {
            auto candidates = extract_hint_candidates(output);
            std::vector<std::string> hint_items;
            // Known type quick-reference hints
            if (output.find("Outcome") != std::string::npos ||
                output.find("Result") != std::string::npos) {
                hint_items.push_back("docs_get(id=\\\"core::error::Outcome\\\")");
            }
            if (output.find("Maybe") != std::string::npos ||
                output.find("Option") != std::string::npos) {
                hint_items.push_back("docs_get(id=\\\"core::types::option::Maybe\\\")");
            }
            if (output.find("Iterator") != std::string::npos) {
                hint_items.push_back("docs_list(module=\\\"core::iter\\\")");
            }
            // Per-candidate search hints (up to 5 additional)
            int count = 0;
            for (const auto& c : candidates) {
                if (count >= 5)
                    break;
                if (c.size() < 3 || c.size() >= 60)
                    continue;
                hint_items.push_back("docs_search(query=\\\"" + json_esc(c) + "\\\")");
                ++count;
            }
            result << "\"docs_hints\":[";
            for (size_t i = 0; i < hint_items.size(); ++i) {
                if (i > 0)
                    result << ",";
                result << "\"" << hint_items[i] << "\"";
            }
            result << "]";
        }

        result << "}";

        return ToolResult::text(result.str());
    }

    std::stringstream result;
    if (exit_code == 0) {
        result << "Tests passed!\n";
    } else {
        result << "Tests failed (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Test Output ---\n" << output;
    }

    if (exit_code != 0) {
        // Append docs hints for types mentioned in test output so the LLM
        // can look up APIs that appear in compiler diagnostics.
        std::string stripped = strip_ansi(output);
        result << build_docs_hints(stripped);
        return ToolResult::error(result.str());
    }

    return ToolResult::text(result.str());
}

auto handle_format(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " format " << file_path;

    auto* check_param = params.get("check");
    if (check_param != nullptr && check_param->is_bool() && check_param->as_bool()) {
        cmd << " --check";
    }

    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Format successful!\n";
    } else {
        result << "Format issues found (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    return ToolResult::text(result.str());
}

auto handle_lint(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " lint " << file_path;

    auto* fix_param = params.get("fix");
    if (fix_param != nullptr && fix_param->is_bool() && fix_param->as_bool()) {
        cmd << " --fix";
    }

    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Lint passed!\n";
    } else {
        result << "Lint found issues (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    // Lint errors are not fatal - return text even with non-zero exit
    return ToolResult::text(result.str());
}

} // namespace tml::mcp
