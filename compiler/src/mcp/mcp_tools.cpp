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

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "doc/doc_model.hpp"
#include "doc/extractor.hpp"
#include "hir/hir_builder.hpp"
#include "mcp_tools_internal.hpp"
#include "mir/hir_mir_builder.hpp"
#include "mir/mir_pass.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace tml::mcp {

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
                     "Return parsed results: total, passed, failed, failures[]", false},
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
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ============================================================================
// Shared Helper: Parse and Type Check
// ============================================================================

auto parse_and_check(const std::string& source, const std::string& filename)
    -> std::variant<CompileContext, CompileError> {
    CompileContext ctx;

    // Preprocess
    preprocessor::Preprocessor pp;
    auto preprocessed = pp.process(source, filename);
    if (!preprocessed.success()) {
        std::string error_msg = "Preprocessing failed:";
        for (const auto& diag : preprocessed.errors()) {
            error_msg += "\n  " + diag.message;
        }
        return CompileError{error_msg};
    }

    // Lex
    auto src = lexer::Source::from_string(preprocessed.output, filename);
    lexer::Lexer lex(src);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        std::string error_msg = "Lexer errors:";
        for (const auto& err : lex.errors()) {
            error_msg += "\n  " + err.message;
        }
        return CompileError{error_msg};
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(filename).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        std::string error_msg = "Parse errors:";
        for (const auto& err : errors) {
            error_msg += "\n  " + err.message;
        }
        return CompileError{error_msg};
    }

    ctx.module = std::move(std::get<parser::Module>(parse_result));

    // Type check
    types::TypeChecker checker;
    auto check_result = checker.check_module(ctx.module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        std::string error_msg = "Type errors:";
        for (const auto& err : errors) {
            error_msg += "\n  " + err.message;
        }
        return CompileError{error_msg};
    }

    ctx.type_env = std::move(std::get<types::TypeEnv>(check_result));

    return ctx;
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
    // Windows: Execute command directly, redirecting stderr to stdout
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = _popen(full_cmd.c_str(), "r");
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
                    _pclose(pipe);
                    return {strip_ansi(output), 124}; // 124 = timeout exit code
                }
            }
        }
        exit_code = _pclose(pipe);
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

    // Read source
    auto source_opt = read_source_file(file_path);
    if (!source_opt.has_value()) {
        return ToolResult::error("Failed to read file: " + file_path);
    }

    // Parse and type check
    auto ctx_result = parse_and_check(*source_opt, file_path);
    if (std::holds_alternative<CompileError>(ctx_result)) {
        return ToolResult::error(std::get<CompileError>(ctx_result).message);
    }

    return ToolResult::text("Type check passed for: " + file_path);
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

    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " build " << file_path << " --emit-ir --legacy";

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
        return ToolResult::error("Failed to emit IR (exit code " + std::to_string(exit_code) +
                                 ")\n" + output);
    }

    // Read the generated .ll file
    fs::path input_path(file_path);
    fs::path ll_path = fs::path("build/debug") / (input_path.stem().string() + ".ll");
    if (!fs::exists(ll_path)) {
        // Try other common locations
        ll_path = input_path.parent_path() / (input_path.stem().string() + ".ll");
    }

    if (!fs::exists(ll_path)) {
        return ToolResult::text("IR generation completed but .ll file not found.\n" + output);
    }

    auto ir_content = read_source_file(ll_path.string());
    if (!ir_content) {
        return ToolResult::error("Failed to read IR file: " + ll_path.string());
    }

    std::string ir = *ir_content;

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

    // Read source
    auto source_opt = read_source_file(file_path);
    if (!source_opt.has_value()) {
        return ToolResult::error("Failed to read file: " + file_path);
    }

    // Parse and type check
    auto ctx_result = parse_and_check(*source_opt, file_path);
    if (std::holds_alternative<CompileError>(ctx_result)) {
        return ToolResult::error(std::get<CompileError>(ctx_result).message);
    }

    auto& ctx = std::get<CompileContext>(ctx_result);

    // Build HIR
    hir::HirBuilder hir_builder(ctx.type_env);
    auto hir_module = hir_builder.lower_module(ctx.module);

    // Build MIR
    mir::HirMirBuilder mir_builder(ctx.type_env);
    auto mir_module = mir_builder.build(hir_module);

    // Serialize MIR to string
    return ToolResult::text(mir::print_module(mir_module));
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

    // Add fail-fast flag
    auto* fail_fast_param = params.get("fail_fast");
    if (fail_fast_param != nullptr && fail_fast_param->is_bool() && fail_fast_param->as_bool()) {
        cmd << " --fail-fast";
    }

    // Execute with longer timeout for test suites
    auto [output, exit_code] = execute_command(cmd.str(), 300);

    // Check if structured output requested
    auto* structured_param = params.get("structured");
    if (structured_param != nullptr && structured_param->is_bool() && structured_param->as_bool()) {
        // Parse test output for structured results
        std::stringstream result;
        result << "{";

        // Parse "test result: ok/FAILED. X passed; Y failed" line
        int total = 0, passed = 0, failed = 0;
        std::vector<std::string> failures;

        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            // Parse result summary
            if (line.find("test result:") != std::string::npos) {
                // Extract numbers
                auto passed_pos = line.find(" passed");
                if (passed_pos != std::string::npos) {
                    auto num_start = line.rfind(' ', passed_pos - 1);
                    if (num_start == std::string::npos)
                        num_start = line.rfind('.', passed_pos - 1);
                    if (num_start != std::string::npos) {
                        try {
                            passed = std::stoi(line.substr(num_start + 1));
                        } catch (...) {}
                    }
                }
                auto failed_pos = line.find(" failed");
                if (failed_pos != std::string::npos) {
                    auto num_start = line.rfind(' ', failed_pos - 1);
                    if (num_start == std::string::npos)
                        num_start = line.rfind(';', failed_pos - 1);
                    if (num_start != std::string::npos) {
                        try {
                            failed = std::stoi(line.substr(num_start + 1));
                        } catch (...) {}
                    }
                }
                total = passed + failed;
            }

            // Collect failure names
            if (line.find("FAILED") != std::string::npos ||
                line.find("FAIL") != std::string::npos) {
                auto test_pos = line.find("test ");
                if (test_pos != std::string::npos) {
                    auto name_start = test_pos + 5;
                    auto name_end = line.find(" ...", name_start);
                    if (name_end == std::string::npos)
                        name_end = line.find(" FAILED", name_start);
                    if (name_end != std::string::npos) {
                        failures.push_back(line.substr(name_start, name_end - name_start));
                    }
                }
            }
        }

        result << "\"total\":" << total << ",";
        result << "\"passed\":" << passed << ",";
        result << "\"failed\":" << failed << ",";
        result << "\"failures\":[";
        for (size_t i = 0; i < failures.size(); ++i) {
            if (i > 0)
                result << ",";
            result << "\"" << failures[i] << "\"";
        }
        result << "]";
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
