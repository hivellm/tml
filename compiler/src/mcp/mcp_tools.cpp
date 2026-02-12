//! # MCP Compiler Tools Implementation
//!
//! Implements the tool handlers for TML compiler operations.
//!
//! ## Integration
//!
//! These tools integrate with the existing compiler infrastructure:
//! - Parser (`parser/parser.hpp`)
//! - Type checker (`types/checker.hpp`)
//! - Code generator (`codegen/llvm_ir_gen.hpp`)
//! - MIR builder (`mir/hir_mir_builder.hpp`)

#include "mcp/mcp_tools.hpp"

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "doc/doc_model.hpp"
#include "doc/extractor.hpp"
#include "hir/hir_builder.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/hir_mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "search/bm25_index.hpp"
#include "search/hnsw_index.hpp"
#include "types/checker.hpp"

#include "json/json_parser.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

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
// Helper: Read File
// ============================================================================

/// Reads a file and returns its contents.
///
/// # Returns
///
/// Optional containing the file contents, or nullopt on error.
static auto read_source_file(const std::string& path) -> std::optional<std::string> {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ============================================================================
// Helper: Parse and Type Check
// ============================================================================

struct CompileContext {
    parser::Module module;
    types::TypeEnv type_env;
};

/// Error type for compilation failures.
struct CompileError {
    std::string message;
};

/// Parses and type-checks TML source code.
///
/// # Arguments
///
/// * `source` - The source code to compile
/// * `filename` - The filename for error messages
///
/// # Returns
///
/// Either a CompileContext on success, or a CompileError on failure.
static auto parse_and_check(const std::string& source, const std::string& filename)
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
// Forward declarations (defined later in file)
// ============================================================================

static auto execute_command(const std::string& cmd, int timeout_seconds = 120)
    -> std::pair<std::string, int>;
static auto get_tml_executable() -> std::string;

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

// ============================================================================
// Helper: Strip ANSI Escape Codes
// ============================================================================

/// Strips ANSI escape sequences (e.g. color codes) from text output.
/// MCP communicates via JSON-RPC — ANSI codes waste tokens and confuse AI parsing.
static auto strip_ansi(const std::string& input) -> std::string {
    std::string out;
    out.reserve(input.size());
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '\033' && i + 1 < input.size() && input[i + 1] == '[') {
            // Skip CSI sequence: ESC [ <params> <final_byte>
            i += 2;
            while (i < input.size() && (static_cast<unsigned char>(input[i]) >= 0x30 &&
                                        static_cast<unsigned char>(input[i]) <= 0x3F)) {
                ++i; // skip parameter bytes (0-9, ;, etc.)
            }
            while (i < input.size() && (static_cast<unsigned char>(input[i]) >= 0x20 &&
                                        static_cast<unsigned char>(input[i]) <= 0x2F)) {
                ++i; // skip intermediate bytes
            }
            if (i < input.size()) {
                ++i; // skip final byte (0x40-0x7E)
            }
        } else {
            out += input[i++];
        }
    }
    return out;
}

// ============================================================================
// Helper: Execute Command and Capture Output
// ============================================================================

/// Executes a command and returns its output (ANSI-stripped) and exit code.
/// If timeout_seconds > 0, kills the process after the specified duration.
static auto execute_command(const std::string& cmd, int timeout_seconds)
    -> std::pair<std::string, int> {
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

/// Gets the path to the TML compiler executable.
static auto get_tml_executable() -> std::string {
    // Try to find tml.exe relative to the current executable or in PATH
    // For MCP, we'll use a well-known location
#ifdef _WIN32
    // Check if tml.exe exists in common locations
    std::vector<std::string> paths = {
        "tml.exe",
        "./build/debug/tml.exe",
        "./build/release/tml.exe",
    };
    for (const auto& path : paths) {
        if (fs::exists(path)) {
            return fs::absolute(path).string();
        }
    }
    // Fall back to PATH
    return "tml.exe";
#else
    return "tml";
#endif
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

    auto& ctx = std::get<CompileContext>(ctx_result);

    // Generate LLVM IR
    codegen::LLVMGenOptions options;
    codegen::LLVMIRGen gen(ctx.type_env, options);
    auto ir_result = gen.generate(ctx.module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(ir_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(ir_result);
        std::string error_msg = "Codegen errors:";
        for (const auto& err : errors) {
            error_msg += "\n  " + err.message;
        }
        return ToolResult::error(error_msg);
    }

    std::string ir = std::get<std::string>(ir_result);

    // Get optional function filter
    auto* function_param = params.get("function");
    if (function_param != nullptr && function_param->is_string()) {
        std::string func_name = function_param->as_string();
        // Extract just the specified function from the IR
        std::string filtered_ir;
        std::istringstream iss(ir);
        std::string line;
        bool in_function = false;
        int brace_depth = 0;

        // Include module header (target info, etc.)
        while (std::getline(iss, line)) {
            if (line.find("define ") != std::string::npos) {
                break; // Stop at first function
            }
            filtered_ir += line + "\n";
        }

        // Rewind and search for the function
        iss.clear();
        iss.seekg(0);
        while (std::getline(iss, line)) {
            if (!in_function) {
                // Look for function definition matching the name
                if (line.find("define ") != std::string::npos &&
                    line.find("@" + func_name) != std::string::npos) {
                    in_function = true;
                    brace_depth = 0;
                    filtered_ir += line + "\n";
                    if (line.find('{') != std::string::npos) {
                        brace_depth++;
                    }
                }
            } else {
                filtered_ir += line + "\n";
                for (char c : line) {
                    if (c == '{')
                        brace_depth++;
                    else if (c == '}')
                        brace_depth--;
                }
                if (brace_depth == 0) {
                    in_function = false;
                    filtered_ir += "\n";
                }
            }
        }

        if (filtered_ir.find("define ") == std::string::npos ||
            filtered_ir.find("@" + func_name) == std::string::npos) {
            return ToolResult::error("Function not found: " + func_name);
        }

        ir = filtered_ir;
    }

    // Get optional offset and limit for chunking
    int64_t offset = 0;
    int64_t limit = -1; // -1 means no limit

    auto* offset_param = params.get("offset");
    if (offset_param != nullptr && offset_param->is_integer()) {
        offset = offset_param->as_i64();
    }

    auto* limit_param = params.get("limit");
    if (limit_param != nullptr && limit_param->is_integer()) {
        limit = limit_param->as_i64();
    }

    // Apply chunking if requested
    if (offset > 0 || limit > 0) {
        std::vector<std::string> lines;
        std::istringstream iss(ir);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }

        size_t total_lines = lines.size();
        size_t start = static_cast<size_t>(std::max(int64_t(0), offset));
        size_t end = total_lines;

        if (limit > 0) {
            end = std::min(start + static_cast<size_t>(limit), total_lines);
        }

        std::stringstream result;
        result << "# Lines " << (start + 1) << "-" << end << " of " << total_lines << "\n";
        result << "# Use offset=" << end << " to continue\n\n";

        for (size_t i = start; i < end; ++i) {
            result << lines[i] << "\n";
        }

        if (end < total_lines) {
            result << "\n# ... " << (total_lines - end) << " more lines\n";
        }

        return ToolResult::text(result.str());
    }

    return ToolResult::text(ir);
}

auto handle_emit_mir(const json::JsonValue& params) -> ToolResult {
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
    // Build command - use the TML executable for test execution
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " test";

    // Get path parameter (optional)
    auto* path_param = params.get("path");
    if (path_param != nullptr && path_param->is_string()) {
        cmd << " " << path_param->as_string();
    }

    // Get filter parameter (optional)
    auto* filter_param = params.get("filter");
    if (filter_param != nullptr && filter_param->is_string()) {
        cmd << " --filter " << filter_param->as_string();
    }

    // Get release parameter (optional)
    auto* release_param = params.get("release");
    if (release_param != nullptr && release_param->is_bool() && release_param->as_bool()) {
        cmd << " --release";
    }

    // Get coverage parameter (optional)
    auto* coverage_param = params.get("coverage");
    if (coverage_param != nullptr && coverage_param->is_bool() && coverage_param->as_bool()) {
        cmd << " --coverage";
    }

    // Get profile parameter (optional)
    auto* profile_param = params.get("profile");
    if (profile_param != nullptr && profile_param->is_bool() && profile_param->as_bool()) {
        cmd << " --profile";
    }

    // Get verbose parameter (optional)
    auto* verbose_param = params.get("verbose");
    if (verbose_param != nullptr && verbose_param->is_bool() && verbose_param->as_bool()) {
        cmd << " --verbose";
    }

    // Get no_cache parameter (optional)
    auto* no_cache_param = params.get("no_cache");
    if (no_cache_param != nullptr && no_cache_param->is_bool() && no_cache_param->as_bool()) {
        cmd << " --no-cache";
    }

    // Get fail_fast parameter (optional)
    auto* fail_fast_param = params.get("fail_fast");
    if (fail_fast_param != nullptr && fail_fast_param->is_bool() && fail_fast_param->as_bool()) {
        cmd << " --fail-fast";
    }

    // Get structured parameter (optional)
    bool structured = false;
    auto* structured_param = params.get("structured");
    if (structured_param != nullptr && structured_param->is_bool()) {
        structured = structured_param->as_bool();
    }

    // Check if coverage was requested (for post-run cleanup)
    bool coverage_requested = false;
    if (coverage_param != nullptr && coverage_param->is_bool() && coverage_param->as_bool()) {
        coverage_requested = true;
    }

    // Execute tests
    auto [output, exit_code] = execute_command(cmd.str());

    // Clean up orphaned coverage temp files if tests failed
    // The suite execution writes to .tmp first and renames on success.
    // If the process crashed, temp files may be left behind.
    if (coverage_requested && exit_code != 0) {
        try {
            // Check common coverage temp file locations
            // The test runner uses CompilerOptions::coverage_output + ".tmp"
            // Default coverage output is "coverage_report.html"
            std::vector<fs::path> tmp_files = {
                "coverage_report.html.tmp",
                "coverage_report.html.json", // JSON temp alongside HTML temp
            };
            for (const auto& tmp : tmp_files) {
                if (fs::exists(tmp)) {
                    fs::remove(tmp);
                }
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    if (structured) {
        // Parse test output into structured format
        int total = 0, passed = 0, failed = 0;
        int files = 0;
        std::string duration;
        std::vector<std::string> failures;

        // Parse line by line
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            // Match: "test <name> ... FAILED (<time>)"
            auto fail_pos = line.find("... FAILED");
            if (fail_pos != std::string::npos) {
                // Extract test name: after "test " until " ..."
                auto test_prefix = line.find("test ");
                if (test_prefix != std::string::npos) {
                    auto name_start = line.find("] ", test_prefix);
                    if (name_start != std::string::npos) {
                        name_start += 2; // skip "] "
                    } else {
                        name_start = test_prefix + 5; // skip "test "
                    }
                    auto name_end = line.find(" ...", name_start);
                    if (name_end != std::string::npos) {
                        failures.push_back(line.substr(name_start, name_end - name_start));
                    }
                }
            }

            // Match: "test result: ok. X passed; Y failed; Z file; finished in Nms"
            auto result_pos = line.find("test result:");
            if (result_pos != std::string::npos) {
                auto after = line.substr(result_pos);
                // Parse passed count
                auto passed_pos = after.find(" passed");
                if (passed_pos != std::string::npos) {
                    // Walk back to find the number
                    auto num_end = passed_pos;
                    auto num_start = num_end;
                    while (num_start > 0 && std::isdigit(after[num_start - 1])) {
                        --num_start;
                    }
                    if (num_start < num_end) {
                        passed = std::stoi(after.substr(num_start, num_end - num_start));
                    }
                }
                // Parse failed count
                auto failed_pos = after.find(" failed");
                if (failed_pos != std::string::npos) {
                    auto num_end = failed_pos;
                    auto num_start = num_end;
                    while (num_start > 0 && std::isdigit(after[num_start - 1])) {
                        --num_start;
                    }
                    if (num_start < num_end) {
                        failed = std::stoi(after.substr(num_start, num_end - num_start));
                    }
                }
                // Parse file count
                auto file_pos = after.find(" file");
                if (file_pos != std::string::npos) {
                    auto num_end = file_pos;
                    auto num_start = num_end;
                    while (num_start > 0 && std::isdigit(after[num_start - 1])) {
                        --num_start;
                    }
                    if (num_start < num_end) {
                        files = std::stoi(after.substr(num_start, num_end - num_start));
                    }
                }
                // Parse duration
                auto fin_pos = after.find("finished in ");
                if (fin_pos != std::string::npos) {
                    duration = after.substr(fin_pos + 12);
                    // Trim trailing whitespace/newline
                    while (!duration.empty() &&
                           (duration.back() == '\n' || duration.back() == '\r' ||
                            duration.back() == ' ')) {
                        duration.pop_back();
                    }
                }
                total = passed + failed;
            }
        }

        // Build structured output
        std::stringstream result;
        result << "{ \"total\": " << total << ", \"passed\": " << passed
               << ", \"failed\": " << failed << ", \"files\": " << files;
        if (!duration.empty()) {
            result << ", \"duration\": \"" << duration << "\"";
        }
        result << ", \"success\": " << (exit_code == 0 ? "true" : "false");
        if (!failures.empty()) {
            result << ", \"failures\": [";
            for (size_t i = 0; i < failures.size(); ++i) {
                if (i > 0)
                    result << ", ";
                result << "\"" << failures[i] << "\"";
            }
            result << "]";
        } else {
            result << ", \"failures\": []";
        }
        result << " }";

        if (exit_code != 0) {
            return ToolResult::error(result.str());
        }
        return ToolResult::text(result.str());
    }

    // Default: human-readable output
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
    // Get file parameter
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    // Check file/directory exists
    if (!fs::exists(file_path)) {
        return ToolResult::error("Path not found: " + file_path);
    }

    // Build command
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " fmt " << file_path;

    // Add check flag if specified
    auto* check_param = params.get("check");
    if (check_param != nullptr && check_param->is_bool() && check_param->as_bool()) {
        cmd << " --check";
    }

    // Check if we're in check-only mode
    bool is_check_mode =
        (check_param != nullptr && check_param->is_bool() && check_param->as_bool());

    // Execute
    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        if (is_check_mode) {
            result << "Already formatted.\n";
        } else {
            result << "Format successful!\n";
        }
        result << "Path: " << file_path << "\n";
    } else if (is_check_mode && exit_code == 1) {
        // In check mode, exit code 1 means "would be reformatted" — not an error
        result << "Needs formatting.\n";
        result << "Path: " << file_path << "\n";
    } else {
        result << "Format failed (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    // In check mode, exit 1 is informational (not an error)
    if (exit_code != 0 && !(is_check_mode && exit_code == 1)) {
        return ToolResult::error(result.str());
    }

    return ToolResult::text(result.str());
}

auto handle_lint(const json::JsonValue& params) -> ToolResult {
    // Get file parameter
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    // Check file/directory exists
    if (!fs::exists(file_path)) {
        return ToolResult::error("Path not found: " + file_path);
    }

    // Build command
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " lint " << file_path;

    // Add fix flag if specified
    auto* fix_param = params.get("fix");
    if (fix_param != nullptr && fix_param->is_bool() && fix_param->as_bool()) {
        cmd << " --fix";
    }

    // Execute
    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Lint passed!\n";
        result << "Path: " << file_path << "\n";
    } else {
        result << "Lint found issues (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    // Lint errors are not fatal - return text even with non-zero exit
    return ToolResult::text(result.str());
}

// ============================================================================
// Documentation Search Infrastructure
// ============================================================================

/// Cached documentation index for the docs/search tool.
/// Built lazily on first query, rebuilt when source files change.
/// Includes BM25 text index and HNSW vector index for hybrid search.
struct DocSearchCache {
    doc::DocIndex index;
    search::BM25Index bm25;
    std::unique_ptr<search::TfIdfVectorizer> vectorizer;
    std::unique_ptr<search::HnswIndex> hnsw;
    /// Flat list of all doc items for doc_id -> DocItem* mapping.
    std::vector<std::pair<const doc::DocItem*, std::string>> all_items;
    std::vector<std::pair<fs::path, fs::file_time_type>> tracked_files;
    bool initialized = false;
    int64_t build_time_ms = 0; // Index build time in milliseconds
    std::mutex mutex;
};

static DocSearchCache g_doc_cache;

/// Discovers the TML project root by walking up from cwd or executable location.
/// Returns empty path if not found.
static auto find_tml_root() -> fs::path {
    // Strategy 1: Walk up from current working directory
    auto cwd = fs::current_path();
    for (auto dir = cwd; dir.has_parent_path() && dir != dir.root_path(); dir = dir.parent_path()) {
        if (fs::exists(dir / "lib" / "core" / "src") && fs::exists(dir / "lib" / "std" / "src")) {
            return dir;
        }
    }

    // Strategy 2: Check relative to executable common locations
    std::vector<fs::path> candidates = {
        cwd / ".." / "..",        // build/debug/ -> root
        cwd / "..",               // build/ -> root
        cwd / ".." / ".." / "..", // build/debug/subdir -> root
    };

    for (const auto& candidate : candidates) {
        auto normalized = fs::weakly_canonical(candidate);
        if (fs::exists(normalized / "lib" / "core" / "src") &&
            fs::exists(normalized / "lib" / "std" / "src")) {
            return normalized;
        }
    }

    return {};
}

/// Collects all .tml source files from a directory recursively.
static auto collect_tml_files(const fs::path& dir) -> std::vector<fs::path> {
    std::vector<fs::path> files;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return files;
    }

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".tml") {
            // Skip test files — they don't contain public API docs
            auto path_str = entry.path().string();
            if (path_str.find("tests") == std::string::npos &&
                path_str.find(".test.") == std::string::npos) {
                files.push_back(entry.path());
            }
        }
    }

    return files;
}

/// Derives a module path from a file path relative to the lib root.
/// e.g. lib/core/src/str/mod.tml -> core::str
///      lib/std/src/json/types.tml -> std::json::types
static auto derive_module_path(const fs::path& file, const fs::path& root) -> std::string {
    auto rel = fs::relative(file, root);
    auto parts = rel.string();

    // Normalize separators
    std::replace(parts.begin(), parts.end(), '\\', '/');

    // Remove lib/ prefix
    if (parts.find("lib/") == 0) {
        parts = parts.substr(4);
    }

    // Remove src/ component
    auto src_pos = parts.find("/src/");
    if (src_pos != std::string::npos) {
        parts = parts.substr(0, src_pos) + "/" + parts.substr(src_pos + 5);
    }

    // Remove .tml extension
    auto ext_pos = parts.rfind(".tml");
    if (ext_pos != std::string::npos) {
        parts = parts.substr(0, ext_pos);
    }

    // Remove /mod suffix (mod.tml represents the parent module)
    if (parts.size() >= 4 && parts.substr(parts.size() - 4) == "/mod") {
        parts = parts.substr(0, parts.size() - 4);
    }

    // Convert / to ::
    std::string module_path;
    for (char c : parts) {
        module_path += (c == '/') ? ':' : c;
    }

    // Fix single : to ::
    std::string result;
    for (size_t i = 0; i < module_path.size(); ++i) {
        result += module_path[i];
        if (module_path[i] == ':' && (i + 1 >= module_path.size() || module_path[i + 1] != ':')) {
            result += ':';
        }
    }

    return result;
}

/// Extracts documentation items from a C++ header file (.hpp).
/// Parses `///` and `//!` doc comments and associates them with
/// function/class/struct/enum declarations following the comments.
///
/// Returns a DocModule with the extracted items, or nullopt if the file
/// has no documentable items.
static auto extract_hpp_docs(const fs::path& file_path, const fs::path& root)
    -> std::optional<doc::DocModule> {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    // Derive module path from compiler/include/X/Y.hpp -> compiler::X::Y
    auto rel = fs::relative(file_path, root / "compiler" / "include");
    std::string mod_name = rel.string();
    std::replace(mod_name.begin(), mod_name.end(), '\\', '/');
    // Remove .hpp extension
    auto ext_pos = mod_name.rfind(".hpp");
    if (ext_pos != std::string::npos) {
        mod_name = mod_name.substr(0, ext_pos);
    }
    // Convert / to :: and prepend compiler::
    std::string mod_path = "compiler::";
    for (char c : mod_name) {
        if (c == '/') {
            mod_path += "::";
        } else {
            mod_path += c;
        }
    }

    doc::DocModule result;
    result.name = file_path.stem().string();
    result.path = mod_path;
    result.source_file = file_path.string();

    std::string line;
    std::string doc_comment;
    std::string module_doc;
    uint32_t line_num = 0;

    while (std::getline(file, line)) {
        ++line_num;

        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            if (!doc_comment.empty()) {
                doc_comment.clear(); // Blank line breaks doc comment
            }
            continue;
        }
        auto trimmed = line.substr(start);

        // Module-level doc comments (//!)
        if (trimmed.find("//!") == 0) {
            auto content = trimmed.substr(3);
            if (!content.empty() && content[0] == ' ') {
                content = content.substr(1);
            }
            module_doc += content + "\n";
            continue;
        }

        // Item-level doc comments (///)
        if (trimmed.find("///") == 0 && (trimmed.size() <= 3 || trimmed[3] != '/')) {
            auto content = trimmed.substr(3);
            if (!content.empty() && content[0] == ' ') {
                content = content.substr(1);
            }
            doc_comment += content + "\n";
            continue;
        }

        // If we have accumulated doc comments, check what follows
        if (!doc_comment.empty()) {
            doc::DocItem item;
            item.doc = doc_comment;
            item.source_file = file_path.string();
            item.source_line = line_num;
            item.path = mod_path;
            item.visibility = doc::DocVisibility::Public;

            // Extract first paragraph as summary
            auto nl_pos = doc_comment.find("\n\n");
            if (nl_pos != std::string::npos) {
                item.summary = doc_comment.substr(0, nl_pos);
            } else {
                item.summary = doc_comment;
                // Trim trailing newline
                while (!item.summary.empty() && item.summary.back() == '\n') {
                    item.summary.pop_back();
                }
            }

            bool found = false;

            // Match: auto function_name(... -> ...
            if (trimmed.find("auto ") != std::string::npos &&
                trimmed.find("->") != std::string::npos) {
                auto auto_pos = trimmed.find("auto ");
                auto name_start = auto_pos + 5;
                auto paren_pos = trimmed.find('(', name_start);
                if (paren_pos != std::string::npos) {
                    item.name = trimmed.substr(name_start, paren_pos - name_start);
                    item.kind = doc::DocItemKind::Function;
                    item.signature = trimmed;
                    // Trim trailing ; or {
                    while (!item.signature.empty() &&
                           (item.signature.back() == ';' || item.signature.back() == '{')) {
                        item.signature.pop_back();
                    }
                    item.id = mod_path + "::" + item.name;
                    found = true;
                }
            }

            // Match: class ClassName or struct ClassName
            if (!found) {
                for (const char* keyword : {"class ", "struct "}) {
                    if (trimmed.find(keyword) == 0) {
                        auto name_start = std::string(keyword).size();
                        std::string name;
                        for (size_t i = name_start; i < trimmed.size(); ++i) {
                            if (trimmed[i] == ' ' || trimmed[i] == '{' || trimmed[i] == ':' ||
                                trimmed[i] == ';') {
                                break;
                            }
                            name += trimmed[i];
                        }
                        if (!name.empty() && name != "}" && name != "=") {
                            item.name = name;
                            item.kind = doc::DocItemKind::Struct;
                            item.signature = std::string(keyword) + name;
                            item.id = mod_path + "::" + item.name;
                            found = true;
                        }
                        break;
                    }
                }
            }

            // Match: enum class EnumName
            if (!found && trimmed.find("enum ") == 0) {
                auto rest = trimmed.substr(5);
                if (rest.find("class ") == 0) {
                    rest = rest.substr(6);
                }
                std::string name;
                for (char c : rest) {
                    if (c == ' ' || c == '{' || c == ':')
                        break;
                    name += c;
                }
                if (!name.empty()) {
                    item.name = name;
                    item.kind = doc::DocItemKind::Enum;
                    item.signature = trimmed;
                    item.id = mod_path + "::" + item.name;
                    found = true;
                }
            }

            // Match: void/int/bool/... function_name(
            if (!found) {
                for (const char* ret : {"void ", "int ", "bool ", "size_t ", "std::string ",
                                        "static auto ", "static void ", "static int "}) {
                    if (trimmed.find(ret) == 0) {
                        auto name_start = std::string(ret).size();
                        auto paren_pos = trimmed.find('(', name_start);
                        if (paren_pos != std::string::npos) {
                            item.name = trimmed.substr(name_start, paren_pos - name_start);
                            item.kind = doc::DocItemKind::Function;
                            item.signature = trimmed;
                            while (!item.signature.empty() &&
                                   (item.signature.back() == ';' || item.signature.back() == '{')) {
                                item.signature.pop_back();
                            }
                            item.id = mod_path + "::" + item.name;
                            found = true;
                        }
                        break;
                    }
                }
            }

            if (found && !item.name.empty()) {
                result.items.push_back(std::move(item));
            }

            doc_comment.clear();
        }
    }

    // Set module-level doc
    if (!module_doc.empty()) {
        result.doc = module_doc;
        auto nl_pos = module_doc.find("\n\n");
        if (nl_pos != std::string::npos) {
            result.summary = module_doc.substr(0, nl_pos);
        } else {
            result.summary = module_doc;
            while (!result.summary.empty() && result.summary.back() == '\n') {
                result.summary.pop_back();
            }
        }
    }

    if (result.items.empty() && result.doc.empty()) {
        return std::nullopt;
    }

    return result;
}

/// Parses a single TML file and extracts documentation (parse-only, no type check).
static auto parse_file_for_docs(const fs::path& file_path) -> std::optional<parser::Module> {
    // Read source
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::stringstream buf;
    buf << file.rdbuf();
    auto source = buf.str();

    // Preprocess
    preprocessor::Preprocessor pp;
    auto preprocessed = pp.process(source, file_path.string());
    if (!preprocessed.success()) {
        return std::nullopt;
    }

    // Lex
    auto src = lexer::Source::from_string(preprocessed.output, file_path.string());
    lexer::Lexer lex(src);
    auto tokens = lex.tokenize();
    if (lex.has_errors()) {
        return std::nullopt;
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = file_path.stem().string();
    auto parse_result = parser.parse_module(module_name);
    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        return std::nullopt;
    }

    return std::move(std::get<parser::Module>(parse_result));
}

/// Checks if any tracked files have changed since the index was built.
static auto files_changed(const DocSearchCache& cache) -> bool {
    for (const auto& [path, mtime] : cache.tracked_files) {
        std::error_code ec;
        auto current_mtime = fs::last_write_time(path, ec);
        if (ec || current_mtime != mtime) {
            return true;
        }
    }
    return false;
}

/// Computes a content fingerprint for all source files.
/// Uses combined file sizes + mtimes as a fast fingerprint.
static auto compute_source_fingerprint(const std::vector<fs::path>& files) -> uint64_t {
    uint64_t hash = 0x517CC1B727220A95ULL; // FNV offset basis
    for (const auto& f : files) {
        std::error_code ec;
        auto sz = fs::file_size(f, ec);
        if (!ec) {
            hash ^= sz;
            hash *= 0x00000100000001B3ULL; // FNV prime
        }
        auto mtime = fs::last_write_time(f, ec);
        if (!ec) {
            auto mtime_val = mtime.time_since_epoch().count();
            hash ^= static_cast<uint64_t>(mtime_val);
            hash *= 0x00000100000001B3ULL;
        }
        // Include file path in hash to detect renames
        for (char c : f.string()) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 0x00000100000001B3ULL;
        }
    }
    return hash;
}

/// Returns the cache directory for persisted indices.
static auto get_cache_dir(const fs::path& root) -> fs::path {
    return root / "build" / "debug" / ".doc-index";
}

/// Saves the BM25, TfIdf, and HNSW indices to disk.
static void save_cached_indices(const DocSearchCache& cache, const fs::path& root,
                                uint64_t fingerprint) {
    auto cache_dir = get_cache_dir(root);
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec)
        return;

    // Save fingerprint
    auto fp_path = cache_dir / "fingerprint.bin";
    {
        std::ofstream out(fp_path, std::ios::binary);
        if (!out)
            return;
        out.write(reinterpret_cast<const char*>(&fingerprint), sizeof(fingerprint));
    }

    // Save BM25 index
    auto bm25_data = cache.bm25.serialize();
    {
        auto bm25_path = cache_dir / "bm25.bin";
        std::ofstream out(bm25_path, std::ios::binary);
        if (out)
            out.write(reinterpret_cast<const char*>(bm25_data.data()), bm25_data.size());
    }

    // Save TfIdf vectorizer
    if (cache.vectorizer) {
        auto tfidf_data = cache.vectorizer->serialize();
        auto tfidf_path = cache_dir / "tfidf.bin";
        std::ofstream out(tfidf_path, std::ios::binary);
        if (out)
            out.write(reinterpret_cast<const char*>(tfidf_data.data()), tfidf_data.size());
    }

    // Save HNSW index
    if (cache.hnsw) {
        auto hnsw_data = cache.hnsw->serialize();
        auto hnsw_path = cache_dir / "hnsw.bin";
        std::ofstream out(hnsw_path, std::ios::binary);
        if (out)
            out.write(reinterpret_cast<const char*>(hnsw_data.data()), hnsw_data.size());
    }
}

/// Reads a binary file into a byte vector.
static auto read_binary_file(const fs::path& path) -> std::vector<uint8_t> {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return {};
    auto size = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

/// Tries to load persisted indices from disk.
/// Returns true if successfully loaded, false if rebuild needed.
static auto load_cached_indices(DocSearchCache& cache, const fs::path& root, uint64_t fingerprint)
    -> bool {
    auto cache_dir = get_cache_dir(root);

    // Check fingerprint
    auto fp_path = cache_dir / "fingerprint.bin";
    if (!fs::exists(fp_path))
        return false;

    uint64_t cached_fp = 0;
    {
        std::ifstream in(fp_path, std::ios::binary);
        if (!in)
            return false;
        in.read(reinterpret_cast<char*>(&cached_fp), sizeof(cached_fp));
    }
    if (cached_fp != fingerprint)
        return false;

    // Load BM25
    auto bm25_data = read_binary_file(cache_dir / "bm25.bin");
    if (bm25_data.empty())
        return false;
    search::BM25Index bm25;
    if (!bm25.deserialize(bm25_data.data(), bm25_data.size()))
        return false;

    // Load TfIdf
    auto tfidf_data = read_binary_file(cache_dir / "tfidf.bin");
    if (tfidf_data.empty())
        return false;
    auto vectorizer = std::make_unique<search::TfIdfVectorizer>(512);
    if (!vectorizer->deserialize(tfidf_data.data(), tfidf_data.size()))
        return false;

    // Load HNSW
    auto hnsw_data = read_binary_file(cache_dir / "hnsw.bin");
    if (hnsw_data.empty())
        return false;
    auto hnsw = std::make_unique<search::HnswIndex>(vectorizer->dims());
    if (!hnsw->deserialize(hnsw_data.data(), hnsw_data.size()))
        return false;

    // All loaded successfully — install into cache
    cache.bm25 = std::move(bm25);
    cache.vectorizer = std::move(vectorizer);
    cache.hnsw = std::move(hnsw);

    return true;
}

/// Builds or rebuilds the documentation index from TML library sources.
/// Uses persisted search indices when source files haven't changed.
static void build_doc_index(DocSearchCache& cache) {
    auto build_start = std::chrono::steady_clock::now();

    auto root = find_tml_root();
    if (root.empty()) {
        return;
    }

    // Collect source files from all library directories dynamically
    std::vector<fs::path> all_files;
    auto lib_root = root / "lib";
    if (fs::exists(lib_root) && fs::is_directory(lib_root)) {
        for (const auto& lib_entry : fs::directory_iterator(lib_root)) {
            if (lib_entry.is_directory()) {
                auto src_dir = lib_entry.path() / "src";
                if (fs::exists(src_dir)) {
                    auto files = collect_tml_files(src_dir);
                    all_files.insert(all_files.end(), files.begin(), files.end());
                }
            }
        }
    }

    // Collect compiler header files for separate doc extraction
    std::vector<fs::path> hpp_files;
    auto compiler_include = root / "compiler" / "include";
    if (fs::exists(compiler_include)) {
        for (const auto& entry : fs::recursive_directory_iterator(compiler_include)) {
            if (entry.is_regular_file() && entry.path().extension() == ".hpp") {
                hpp_files.push_back(entry.path());
            }
        }
    }

    // Compute source fingerprint for cache validation (include both .tml and .hpp files)
    std::vector<fs::path> all_tracked_files = all_files;
    all_tracked_files.insert(all_tracked_files.end(), hpp_files.begin(), hpp_files.end());
    auto fingerprint = compute_source_fingerprint(all_tracked_files);

    // Parse each file and extract documentation
    // (always needed for the DocItem pointers in all_items)
    doc::ExtractorConfig config;
    config.include_private = false;
    config.extract_examples = true;
    doc::Extractor extractor(config);

    std::vector<std::pair<const parser::Module*, std::string>> module_pairs;
    std::vector<parser::Module> parsed_modules; // Keep alive for pointers
    parsed_modules.reserve(all_files.size());

    cache.tracked_files.clear();

    for (const auto& file : all_files) {
        auto module_opt = parse_file_for_docs(file);
        if (!module_opt) {
            continue;
        }

        parsed_modules.push_back(std::move(*module_opt));
        auto module_path = derive_module_path(file, root);
        module_pairs.push_back({&parsed_modules.back(), module_path});

        // Track file modification time
        std::error_code ec;
        auto mtime = fs::last_write_time(file, ec);
        if (!ec) {
            cache.tracked_files.push_back({file, mtime});
        }
    }

    if (module_pairs.empty() && hpp_files.empty()) {
        return;
    }

    // Build the doc index from TML library sources
    if (!module_pairs.empty()) {
        cache.index = extractor.extract_all(module_pairs);
    }

    // Extract documentation from compiler C++ headers
    for (const auto& hpp_file : hpp_files) {
        auto hpp_mod = extract_hpp_docs(hpp_file, root);
        if (hpp_mod && (!hpp_mod->items.empty() || !hpp_mod->doc.empty())) {
            cache.index.modules.push_back(std::move(*hpp_mod));
        }

        // Track hpp file modification time
        std::error_code ec;
        auto mtime = fs::last_write_time(hpp_file, ec);
        if (!ec) {
            cache.tracked_files.push_back({hpp_file, mtime});
        }
    }

    // Flatten all items for doc_id mapping
    cache.all_items.clear();

    std::function<void(const std::vector<doc::DocItem>&, const std::string&)> collect_items;
    collect_items = [&](const std::vector<doc::DocItem>& items, const std::string& mod_path) {
        for (const auto& item : items) {
            cache.all_items.push_back({&item, mod_path});
            collect_items(item.methods, mod_path);
            collect_items(item.fields, mod_path);
            collect_items(item.variants, mod_path);
        }
    };

    std::function<void(const std::vector<doc::DocModule>&)> collect_modules;
    collect_modules = [&](const std::vector<doc::DocModule>& modules) {
        for (const auto& mod : modules) {
            collect_items(mod.items, mod.path);
            collect_modules(mod.submodules);
        }
    };

    collect_modules(cache.index.modules);

    // Try loading persisted search indices (BM25 + TfIdf + HNSW)
    bool loaded_from_cache = load_cached_indices(cache, root, fingerprint);

    if (loaded_from_cache) {
        // Verify the cached indices match the current item count
        if (cache.bm25.size() == cache.all_items.size() && cache.bm25.is_built()) {
            cache.initialized = true;
            auto build_end = std::chrono::steady_clock::now();
            cache.build_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start)
                    .count();
            return; // Cache hit — skip expensive BM25/HNSW build
        }
        // Mismatch — fall through to rebuild
    }

    // Full rebuild: feed items to BM25 + HNSW
    cache.bm25 = search::BM25Index{};
    cache.vectorizer = std::make_unique<search::TfIdfVectorizer>(512);

    uint32_t doc_id = 0;
    for (const auto& [item, mod_path] : cache.all_items) {
        std::string combined = item->name + " " + item->signature + " " + item->doc + " " +
                               item->path + " " + mod_path;
        cache.bm25.add_document(doc_id, item->name, item->signature, item->doc, item->path);
        cache.vectorizer->add_document(doc_id, combined);
        doc_id++;
    }

    // Build BM25 index
    cache.bm25.build();

    // Build HNSW: vectorize all documents and insert
    cache.vectorizer->build();
    size_t dims = cache.vectorizer->dims();

    if (dims > 0) {
        cache.hnsw = std::make_unique<search::HnswIndex>(dims);
        cache.hnsw->set_params(16, 200, 50);

        for (uint32_t i = 0; i < static_cast<uint32_t>(cache.all_items.size()); ++i) {
            const auto& [item, mod_path] = cache.all_items[i];
            std::string combined = item->name + " " + item->signature + " " + item->doc + " " +
                                   item->path + " " + mod_path;
            auto vec = cache.vectorizer->vectorize(combined);
            cache.hnsw->insert(i, vec);
        }
    }

    cache.initialized = true;

    auto build_end = std::chrono::steady_clock::now();
    cache.build_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start).count();

    // Persist indices to disk for next startup
    save_cached_indices(cache, root, fingerprint);
}

/// Ensures the doc index is built and up-to-date.
static void ensure_doc_index() {
    std::lock_guard<std::mutex> lock(g_doc_cache.mutex);

    if (!g_doc_cache.initialized || files_changed(g_doc_cache)) {
        build_doc_index(g_doc_cache);
    }
}

/// Case-insensitive substring search.
static auto icontains(const std::string& haystack, const std::string& needle) -> bool {
    if (needle.empty()) {
        return true;
    }
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}

/// Converts a string to a DocItemKind filter, or nullopt if invalid.
static auto parse_kind_filter(const std::string& kind) -> std::optional<doc::DocItemKind> {
    std::string k = kind;
    std::transform(k.begin(), k.end(), k.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (k == "function" || k == "func")
        return doc::DocItemKind::Function;
    if (k == "method")
        return doc::DocItemKind::Method;
    if (k == "struct" || k == "type")
        return doc::DocItemKind::Struct;
    if (k == "enum")
        return doc::DocItemKind::Enum;
    if (k == "behavior" || k == "trait")
        return doc::DocItemKind::Trait;
    if (k == "constant" || k == "const")
        return doc::DocItemKind::Constant;
    if (k == "field")
        return doc::DocItemKind::Field;
    if (k == "variant")
        return doc::DocItemKind::Variant;
    if (k == "impl")
        return doc::DocItemKind::Impl;
    if (k == "module")
        return doc::DocItemKind::Module;
    return std::nullopt;
}

/// A scored search result entry.
struct ScoredDocResult {
    const doc::DocItem* item;
    std::string module_path;
    float score;
    float bm25_contribution = 0.0f; // Score breakdown: BM25 portion
    float hnsw_contribution = 0.0f; // Score breakdown: HNSW portion
    float signal_boost = 0.0f;      // Score breakdown: multi-signal boost
};

/// Formats a single search result for display.
static void format_result(std::stringstream& out, const ScoredDocResult& result) {
    const auto& item = *result.item;
    auto kind_str = doc::doc_item_kind_to_string(item.kind);

    out << "=== " << item.path << " (" << kind_str << ") ===\n";

    if (!item.signature.empty()) {
        out << "  Signature: " << item.signature << "\n";
    }

    out << "  Module:    " << result.module_path << "\n";

    if (!item.source_file.empty()) {
        out << "  Source:    " << item.source_file;
        if (item.source_line > 0) {
            out << ":" << item.source_line;
        }
        out << "\n";
    }

    if (!item.summary.empty()) {
        out << "\n  " << item.summary << "\n";
    } else if (!item.doc.empty()) {
        // Show first 200 chars of doc if no summary
        auto doc_preview = item.doc.substr(0, 200);
        if (item.doc.size() > 200) {
            doc_preview += "...";
        }
        out << "\n  " << doc_preview << "\n";
    }

    // Show parameters for functions/methods
    if (!item.params.empty() &&
        (item.kind == doc::DocItemKind::Function || item.kind == doc::DocItemKind::Method)) {
        bool has_desc = false;
        for (const auto& param : item.params) {
            if (!param.description.empty()) {
                has_desc = true;
                break;
            }
        }
        if (has_desc) {
            out << "\n  Parameters:\n";
            for (const auto& param : item.params) {
                if (param.name == "this")
                    continue;
                out << "    " << param.name;
                if (!param.type.empty()) {
                    out << ": " << param.type;
                }
                if (!param.description.empty()) {
                    out << " - " << param.description;
                }
                out << "\n";
            }
        }
    }

    // Show return type
    if (item.returns && !item.returns->description.empty()) {
        out << "  Returns: " << item.returns->description << "\n";
    }

    // Show deprecation warning
    if (item.deprecated) {
        out << "\n  [DEPRECATED] " << item.deprecated->message << "\n";
    }

    // Score breakdown (for debugging/transparency)
    if (result.bm25_contribution > 0.0f || result.hnsw_contribution > 0.0f ||
        result.signal_boost > 0.0f) {
        out << "  Score: " << std::fixed << std::setprecision(4) << result.score;
        out << " (";
        bool first = true;
        if (result.bm25_contribution > 0.0f) {
            out << "BM25=" << result.bm25_contribution;
            first = false;
        }
        if (result.hnsw_contribution > 0.0f) {
            if (!first)
                out << ", ";
            out << "HNSW=" << result.hnsw_contribution;
            first = false;
        }
        if (result.signal_boost > 0.0f) {
            if (!first)
                out << ", ";
            out << "boost=" << result.signal_boost;
        }
        out << ")\n";
    }

    out << "\n";
}

/// Reciprocal Rank Fusion: merges two ranked result lists.
/// RRF score = sum(weight / (k + rank)) for each list where the item appears.
/// BM25 gets 2x weight since keyword matches are more precise for doc search.
/// HNSW-only results (no BM25 match) require very low distance to be included,
/// preventing noisy semantic results from polluting keyword searches.
static auto reciprocal_rank_fusion(const std::vector<search::BM25Result>& bm25_results,
                                   const std::vector<search::HnswResult>& hnsw_results,
                                   size_t limit) -> std::vector<ScoredDocResult> {
    const float k = 60.0f;          // Standard RRF constant
    const float bm25_weight = 2.0f; // BM25 is more precise for keyword search
    const float hnsw_weight = 1.0f;
    const float hnsw_boost_cutoff = 0.8f;      // HNSW results close enough to boost BM25 matches
    const float hnsw_standalone_cutoff = 0.5f; // HNSW-only results need very high similarity

    // Track which doc_ids appear in BM25 results
    std::unordered_set<uint32_t> bm25_doc_ids;
    for (const auto& r : bm25_results) {
        bm25_doc_ids.insert(r.doc_id);
    }

    // Map doc_id -> fused score
    std::unordered_map<uint32_t, float> fused_scores;

    for (size_t rank = 0; rank < bm25_results.size(); ++rank) {
        fused_scores[bm25_results[rank].doc_id] += bm25_weight / (k + static_cast<float>(rank + 1));
    }

    for (size_t rank = 0; rank < hnsw_results.size(); ++rank) {
        auto doc_id = hnsw_results[rank].doc_id;
        float distance = hnsw_results[rank].distance;

        bool in_bm25 = bm25_doc_ids.count(doc_id) > 0;

        if (in_bm25 && distance < hnsw_boost_cutoff) {
            // Boost BM25 matches that also have good semantic similarity
            fused_scores[doc_id] += hnsw_weight / (k + static_cast<float>(rank + 1));
        } else if (!in_bm25 && distance < hnsw_standalone_cutoff) {
            // Only include HNSW-only results if they are very semantically similar
            fused_scores[doc_id] += hnsw_weight / (k + static_cast<float>(rank + 1));
        }
        // Otherwise: skip noisy HNSW results
    }

    // Build result list
    std::vector<ScoredDocResult> results;
    results.reserve(fused_scores.size());

    for (const auto& [doc_id, score] : fused_scores) {
        if (doc_id < g_doc_cache.all_items.size()) {
            const auto& [item, mod_path] = g_doc_cache.all_items[doc_id];
            results.push_back({item, mod_path, score});
        }
    }

    // Sort by score descending
    std::sort(results.begin(), results.end(),
              [](const ScoredDocResult& a, const ScoredDocResult& b) { return a.score > b.score; });

    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

/// Applies kind and module filters to a result set.
static void apply_filters(std::vector<ScoredDocResult>& results,
                          std::optional<doc::DocItemKind> kind_filter,
                          const std::string& module_filter) {
    if (!kind_filter && module_filter.empty())
        return;

    results.erase(std::remove_if(results.begin(), results.end(),
                                 [&](const ScoredDocResult& r) {
                                     if (kind_filter && r.item->kind != *kind_filter)
                                         return true;
                                     if (!module_filter.empty() &&
                                         !icontains(r.module_path, module_filter) &&
                                         !icontains(r.item->path, module_filter))
                                         return true;
                                     return false;
                                 }),
                  results.end());
}

// ============================================================================
// Phase 8: Query Processing (expansion, synonyms, stop words)
// ============================================================================

/// TML-specific synonym map for query expansion.
/// Maps common search terms to their TML equivalents.
static const std::unordered_map<std::string, std::vector<std::string>>& get_tml_synonyms() {
    static const std::unordered_map<std::string, std::vector<std::string>> synonyms = {
        {"error", {"Outcome", "Err", "Result"}},
        {"result", {"Outcome", "Ok", "Err"}},
        {"optional", {"Maybe", "Just", "Nothing"}},
        {"option", {"Maybe", "Just", "Nothing"}},
        {"none", {"Nothing", "Maybe"}},
        {"some", {"Just", "Maybe"}},
        {"null", {"Nothing", "Maybe"}},
        {"nullable", {"Maybe", "Just", "Nothing"}},
        {"box", {"Heap"}},
        {"heap", {"Heap", "alloc"}},
        {"rc", {"Shared"}},
        {"arc", {"Sync"}},
        {"clone", {"duplicate", "Duplicate"}},
        {"trait", {"behavior"}},
        {"interface", {"behavior"}},
        {"unsafe", {"lowlevel"}},
        {"match", {"when"}},
        {"switch", {"when"}},
        {"for", {"loop", "iter"}},
        {"while", {"loop"}},
        {"fn", {"func"}},
        {"function", {"func"}},
        {"string", {"Str", "str"}},
        {"vector", {"List"}},
        {"vec", {"List"}},
        {"array", {"List", "Array"}},
        {"map", {"HashMap"}},
        {"hashmap", {"HashMap"}},
        {"dict", {"HashMap"}},
        {"dictionary", {"HashMap"}},
        {"set", {"HashSet"}},
        {"hashset", {"HashSet"}},
        {"mutex", {"Mutex", "sync"}},
        {"lock", {"Mutex", "sync"}},
        {"thread", {"thread", "spawn"}},
        {"async", {"async", "Future"}},
        {"future", {"Future", "async"}},
        {"print", {"print", "println", "fmt"}},
        {"format", {"fmt", "format", "Display"}},
        {"display", {"Display", "fmt", "to_str"}},
        {"debug", {"Debug", "fmt"}},
        {"hash", {"Hash", "fnv", "murmur"}},
        {"json", {"Json", "JsonValue", "parse"}},
        {"file", {"File", "read", "write", "open"}},
        {"socket", {"TcpStream", "TcpListener", "net"}},
        {"http", {"net", "TcpStream"}},
        {"encrypt", {"crypto", "aes", "sha"}},
        {"crypto", {"crypto", "sha256", "aes"}},
        {"compress", {"zlib", "gzip", "deflate"}},
        {"sort", {"sort", "sorted", "cmp", "Ordering"}},
        {"compare", {"cmp", "Ordering", "PartialOrd"}},
        {"iterator", {"iter", "Iterator", "next"}},
        {"range", {"to", "through", "Range"}},
        {"slice", {"slice", "Slice"}},
        {"convert", {"From", "Into", "as"}},
        {"cast", {"as", "From", "Into"}},
        {"log", {"log", "info", "warn", "error", "debug"}},
        {"logging", {"log", "Logger"}},
    };
    return synonyms;
}

/// Query stop words to remove before searching.
static const std::unordered_set<std::string>& get_query_stop_words() {
    static const std::unordered_set<std::string> stops = {
        "the",     "a",      "an",     "is",    "are",   "was",  "were",  "be",    "been",
        "being",   "have",   "has",    "had",   "do",    "does", "did",   "will",  "would",
        "shall",   "should", "may",    "might", "must",  "can",  "could", "in",    "on",
        "at",      "to",     "for",    "of",    "with",  "by",   "from",  "as",    "into",
        "through", "during", "before", "after", "about", "i",    "me",    "my",    "we",
        "our",     "you",    "your",   "it",    "its",   "this", "that",  "these", "those",
        "what",    "which",  "who",    "how",   "where", "when", "why",   "and",   "or",
        "but",     "not",    "no",     "nor",   "all",   "each", "every", "any",   "both",
        "tml",     "use",    "using",
    };
    return stops;
}

/// Processes a query: removes stop words and expands with TML synonyms.
/// Returns a list of queries to search (original cleaned + expanded variants).
static auto process_query(const std::string& raw_query) -> std::vector<std::string> {
    std::vector<std::string> queries;
    const auto& stops = get_query_stop_words();
    const auto& synonyms = get_tml_synonyms();

    // Tokenize and clean the query
    std::string lower_query;
    lower_query.reserve(raw_query.size());
    for (char c : raw_query) {
        lower_query += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::vector<std::string> tokens;
    std::istringstream iss(lower_query);
    std::string token;
    while (iss >> token) {
        // Strip non-alphanumeric from edges
        while (!token.empty() && !std::isalnum(static_cast<unsigned char>(token.front()))) {
            token.erase(token.begin());
        }
        while (!token.empty() && !std::isalnum(static_cast<unsigned char>(token.back()))) {
            token.pop_back();
        }
        if (!token.empty() && stops.find(token) == stops.end()) {
            tokens.push_back(token);
        }
    }

    // Build cleaned query (stop words removed)
    std::string cleaned;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0)
            cleaned += " ";
        cleaned += tokens[i];
    }

    // Always include the original raw query (BM25 tokenizer handles its own splitting)
    queries.push_back(raw_query);

    // Add cleaned query if different
    if (!cleaned.empty() && cleaned != raw_query) {
        queries.push_back(cleaned);
    }

    // Expand each token with TML synonyms
    for (const auto& tok : tokens) {
        auto it = synonyms.find(tok);
        if (it != synonyms.end()) {
            for (const auto& syn : it->second) {
                // Add each synonym as a standalone query
                queries.push_back(syn);
                // Also combine synonym with other tokens for context
                if (tokens.size() > 1) {
                    std::string combined;
                    for (const auto& t : tokens) {
                        if (t == tok) {
                            combined += syn;
                        } else {
                            combined += t;
                        }
                        combined += " ";
                    }
                    if (!combined.empty())
                        combined.pop_back();
                    queries.push_back(combined);
                }
            }
        }
    }

    // Deduplicate
    std::unordered_set<std::string> seen;
    std::vector<std::string> unique;
    for (auto& q : queries) {
        if (seen.insert(q).second) {
            unique.push_back(std::move(q));
        }
    }

    // Limit to 8 queries max (original + 7 expansions)
    if (unique.size() > 8) {
        unique.resize(8);
    }

    return unique;
}

/// Multi-query fusion: search multiple expanded queries and merge results.
/// Each result keeps its best score across all queries.
static auto multi_query_search(const std::vector<std::string>& queries, const std::string& mode,
                               size_t fetch_limit) -> std::vector<ScoredDocResult> {
    std::unordered_map<uint32_t, ScoredDocResult> best_results;

    for (size_t qi = 0; qi < queries.size(); ++qi) {
        const auto& q = queries[qi];
        // Weight: original query gets full weight, expansions get diminishing weight
        float query_weight = (qi == 0) ? 1.0f : 0.6f;

        if (mode == "text") {
            auto bm25_results = g_doc_cache.bm25.search(q, fetch_limit);
            for (const auto& r : bm25_results) {
                if (r.doc_id < g_doc_cache.all_items.size()) {
                    float weighted = r.score * query_weight;
                    auto it = best_results.find(r.doc_id);
                    if (it == best_results.end() || weighted > it->second.score) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        best_results[r.doc_id] = {item, mod_path, weighted, weighted, 0.0f, 0.0f};
                    }
                }
            }
        } else if (mode == "semantic" && g_doc_cache.hnsw && g_doc_cache.vectorizer) {
            auto query_vec = g_doc_cache.vectorizer->vectorize(q);
            auto hnsw_results = g_doc_cache.hnsw->search(query_vec, fetch_limit);
            for (const auto& r : hnsw_results) {
                if (r.doc_id < g_doc_cache.all_items.size()) {
                    float sim = (1.0f - r.distance) * query_weight;
                    auto it = best_results.find(r.doc_id);
                    if (it == best_results.end() || sim > it->second.score) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        best_results[r.doc_id] = {item, mod_path, sim, 0.0f, sim, 0.0f};
                    }
                }
            }
        } else {
            // Hybrid: run both and fuse per query
            auto bm25_results = g_doc_cache.bm25.search(q, fetch_limit);

            if (g_doc_cache.hnsw && g_doc_cache.vectorizer) {
                auto query_vec = g_doc_cache.vectorizer->vectorize(q);
                auto hnsw_results = g_doc_cache.hnsw->search(query_vec, fetch_limit);
                auto fused = reciprocal_rank_fusion(bm25_results, hnsw_results, fetch_limit);
                for (auto& r : fused) {
                    // Find the doc_id by scanning all_items
                    for (uint32_t did = 0; did < g_doc_cache.all_items.size(); ++did) {
                        if (g_doc_cache.all_items[did].first == r.item) {
                            float weighted = r.score * query_weight;
                            auto it = best_results.find(did);
                            if (it == best_results.end() || weighted > it->second.score) {
                                r.score = weighted;
                                best_results[did] = r;
                            }
                            break;
                        }
                    }
                }
            } else {
                for (const auto& r : bm25_results) {
                    if (r.doc_id < g_doc_cache.all_items.size()) {
                        float weighted = r.score * query_weight;
                        auto it = best_results.find(r.doc_id);
                        if (it == best_results.end() || weighted > it->second.score) {
                            const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                            best_results[r.doc_id] = {item,     mod_path, weighted,
                                                      weighted, 0.0f,     0.0f};
                        }
                    }
                }
            }
        }
    }

    // Convert map to vector and sort
    std::vector<ScoredDocResult> results;
    results.reserve(best_results.size());
    for (auto& [_, r] : best_results) {
        results.push_back(std::move(r));
    }
    std::sort(results.begin(), results.end(),
              [](const ScoredDocResult& a, const ScoredDocResult& b) { return a.score > b.score; });

    if (results.size() > fetch_limit) {
        results.resize(fetch_limit);
    }

    return results;
}

// ============================================================================
// Phase 9: MMR Diversification
// ============================================================================

/// Computes Jaccard similarity between two text strings (word-set based).
static auto jaccard_similarity(const std::string& a, const std::string& b) -> float {
    std::unordered_set<std::string> words_a, words_b;

    auto tokenize = [](const std::string& text, std::unordered_set<std::string>& words) {
        std::string lower;
        lower.reserve(text.size());
        for (char c : text) {
            lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        std::istringstream iss(lower);
        std::string word;
        while (iss >> word) {
            if (word.size() >= 2) {
                words.insert(word);
            }
        }
    };

    tokenize(a, words_a);
    tokenize(b, words_b);

    if (words_a.empty() && words_b.empty())
        return 0.0f;

    size_t intersection = 0;
    for (const auto& w : words_a) {
        if (words_b.count(w))
            ++intersection;
    }

    size_t union_size = words_a.size() + words_b.size() - intersection;
    if (union_size == 0)
        return 0.0f;

    return static_cast<float>(intersection) / static_cast<float>(union_size);
}

/// Builds a content string for an item (for similarity comparison).
static auto item_content(const ScoredDocResult& r) -> std::string {
    return r.item->name + " " + r.item->signature + " " + r.module_path;
}

/// MMR (Maximal Marginal Relevance) diversification.
/// Reranks results to balance relevance and diversity.
/// lambda = 1.0 -> pure relevance, lambda = 0.0 -> pure diversity.
static void mmr_diversify(std::vector<ScoredDocResult>& results, float lambda = 0.7f) {
    if (results.size() <= 2)
        return;

    std::vector<ScoredDocResult> diversified;
    diversified.reserve(results.size());

    // First result is always the top-scored one
    diversified.push_back(std::move(results[0]));
    results.erase(results.begin());

    // Pre-compute content strings for remaining
    std::vector<std::string> contents;
    contents.reserve(results.size());
    for (const auto& r : results) {
        contents.push_back(item_content(r));
    }

    std::vector<std::string> selected_contents;
    selected_contents.push_back(item_content(diversified[0]));

    while (!results.empty() && diversified.size() < diversified.capacity()) {
        float best_mmr = -1e9f;
        size_t best_idx = 0;

        for (size_t i = 0; i < results.size(); ++i) {
            // Find max similarity to any already-selected result
            float max_sim = 0.0f;
            for (const auto& sel_content : selected_contents) {
                float sim = jaccard_similarity(contents[i], sel_content);
                if (sim > max_sim)
                    max_sim = sim;
            }

            // MMR score: balance relevance vs diversity
            float mmr = lambda * results[i].score - (1.0f - lambda) * max_sim;
            if (mmr > best_mmr) {
                best_mmr = mmr;
                best_idx = i;
            }
        }

        selected_contents.push_back(contents[best_idx]);
        diversified.push_back(std::move(results[best_idx]));
        results.erase(results.begin() + static_cast<ptrdiff_t>(best_idx));
        contents.erase(contents.begin() + static_cast<ptrdiff_t>(best_idx));
    }

    results = std::move(diversified);
}

/// Deduplicates near-identical results using Jaccard threshold.
static void deduplicate_results(std::vector<ScoredDocResult>& results, float threshold = 0.8f) {
    if (results.size() <= 1)
        return;

    std::vector<ScoredDocResult> deduped;
    deduped.reserve(results.size());

    for (auto& r : results) {
        bool is_dup = false;
        std::string content = item_content(r);
        for (const auto& kept : deduped) {
            if (jaccard_similarity(content, item_content(kept)) > threshold) {
                is_dup = true;
                break;
            }
        }
        if (!is_dup) {
            deduped.push_back(std::move(r));
        }
    }

    results = std::move(deduped);
}

// ============================================================================
// Phase 10: Multi-Signal Ranking Boost
// ============================================================================

/// Applies multi-signal ranking boosts to results.
/// Boosts pub items, well-documented items, and top-level module items.
static void apply_signal_boosts(std::vector<ScoredDocResult>& results) {
    for (auto& r : results) {
        float boost = 0.0f;

        // Boost pub items (have "pub" in signature)
        if (!r.item->signature.empty() && r.item->signature.find("pub ") != std::string::npos) {
            boost += 0.005f;
        }

        // Boost well-documented items (have doc comments)
        if (!r.item->doc.empty()) {
            boost += 0.003f;
            // Extra boost for items with parameter docs
            if (!r.item->params.empty()) {
                bool has_param_docs = false;
                for (const auto& p : r.item->params) {
                    if (!p.description.empty()) {
                        has_param_docs = true;
                        break;
                    }
                }
                if (has_param_docs) {
                    boost += 0.002f;
                }
            }
        }

        // Boost top-level module items (fewer :: separators = more prominent)
        {
            size_t depth = 0;
            for (size_t i = 0; i + 1 < r.module_path.size(); ++i) {
                if (r.module_path[i] == ':' && r.module_path[i + 1] == ':') {
                    ++depth;
                    ++i; // skip second ':'
                }
            }
            // Top-level (depth 1 like "core::str") gets more boost
            if (depth <= 1) {
                boost += 0.003f;
            } else if (depth == 2) {
                boost += 0.001f;
            }
        }

        r.signal_boost = boost;
        r.score += boost;
    }

    // Re-sort after boosting
    std::sort(results.begin(), results.end(),
              [](const ScoredDocResult& a, const ScoredDocResult& b) { return a.score > b.score; });
}

// ============================================================================
// Search Handler
// ============================================================================

auto handle_docs_search(const json::JsonValue& params) -> ToolResult {
    // Get query parameter
    auto* query_param = params.get("query");
    if (query_param == nullptr || !query_param->is_string()) {
        return ToolResult::error("Missing or invalid 'query' parameter");
    }
    std::string query = query_param->as_string();

    // Get limit parameter (optional)
    int64_t limit = 10;
    auto* limit_param = params.get("limit");
    if (limit_param != nullptr && limit_param->is_integer()) {
        limit = limit_param->as_i64();
    }

    // Get kind filter (optional)
    std::optional<doc::DocItemKind> kind_filter;
    auto* kind_param = params.get("kind");
    if (kind_param != nullptr && kind_param->is_string()) {
        kind_filter = parse_kind_filter(kind_param->as_string());
        if (!kind_filter) {
            return ToolResult::error(
                "Invalid 'kind' parameter. Valid values: function, method, struct, enum, "
                "behavior, constant, field, variant");
        }
    }

    // Get module filter (optional)
    std::string module_filter;
    auto* module_param = params.get("module");
    if (module_param != nullptr && module_param->is_string()) {
        module_filter = module_param->as_string();
    }

    // Get search mode (optional, default: hybrid)
    std::string mode = "hybrid";
    auto* mode_param = params.get("mode");
    if (mode_param != nullptr && mode_param->is_string()) {
        mode = mode_param->as_string();
        if (mode != "text" && mode != "semantic" && mode != "hybrid") {
            return ToolResult::error(
                "Invalid 'mode' parameter. Valid values: text, semantic, hybrid");
        }
    }

    // Ensure the documentation index is built
    ensure_doc_index();

    std::stringstream output;

    if (!g_doc_cache.initialized) {
        output << "Documentation index not available.\n";
        output << "Could not locate TML library sources.\n";
        output << "Ensure the MCP server is run from the TML project directory.\n";
        return ToolResult::text(output.str());
    }

    auto search_start = std::chrono::steady_clock::now();

    std::vector<ScoredDocResult> results;
    size_t fetch_limit = static_cast<size_t>(limit) * 3; // Over-fetch before filtering

    // Phase 8: Query processing — expand with synonyms and clean stop words
    auto expanded_queries = process_query(query);
    bool used_expansion = expanded_queries.size() > 1;

    if (expanded_queries.size() > 1) {
        // Multi-query fusion: search all expanded queries and merge
        results = multi_query_search(expanded_queries, mode, fetch_limit);
    } else {
        // Single query path (original behavior)
        if (mode == "text") {
            auto bm25_results = g_doc_cache.bm25.search(query, fetch_limit);
            results.reserve(bm25_results.size());
            for (const auto& r : bm25_results) {
                if (r.doc_id < g_doc_cache.all_items.size()) {
                    const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                    results.push_back({item, mod_path, r.score, r.score, 0.0f, 0.0f});
                }
            }
        } else if (mode == "semantic") {
            if (g_doc_cache.hnsw && g_doc_cache.vectorizer) {
                auto query_vec = g_doc_cache.vectorizer->vectorize(query);
                auto hnsw_results = g_doc_cache.hnsw->search(query_vec, fetch_limit);
                results.reserve(hnsw_results.size());
                for (const auto& r : hnsw_results) {
                    if (r.doc_id < g_doc_cache.all_items.size()) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        float sim = 1.0f - r.distance;
                        results.push_back({item, mod_path, sim, 0.0f, sim, 0.0f});
                    }
                }
            } else {
                auto bm25_results = g_doc_cache.bm25.search(query, fetch_limit);
                for (const auto& r : bm25_results) {
                    if (r.doc_id < g_doc_cache.all_items.size()) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        results.push_back({item, mod_path, r.score, r.score, 0.0f, 0.0f});
                    }
                }
            }
        } else {
            auto bm25_results = g_doc_cache.bm25.search(query, fetch_limit);
            if (g_doc_cache.hnsw && g_doc_cache.vectorizer) {
                auto query_vec = g_doc_cache.vectorizer->vectorize(query);
                auto hnsw_results = g_doc_cache.hnsw->search(query_vec, fetch_limit);
                results = reciprocal_rank_fusion(bm25_results, hnsw_results, fetch_limit);
            } else {
                for (const auto& r : bm25_results) {
                    if (r.doc_id < g_doc_cache.all_items.size()) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        results.push_back({item, mod_path, r.score, r.score, 0.0f, 0.0f});
                    }
                }
            }
        }
    }

    // Apply kind and module filters
    apply_filters(results, kind_filter, module_filter);

    // Phase 10: Multi-signal ranking boosts (pub, documented, top-level)
    apply_signal_boosts(results);

    // Phase 9: Deduplicate near-identical results, then MMR diversify
    deduplicate_results(results);
    mmr_diversify(results);

    // Apply final limit
    if (results.size() > static_cast<size_t>(limit)) {
        results.resize(static_cast<size_t>(limit));
    }

    auto search_end = std::chrono::steady_clock::now();
    auto search_ms =
        std::chrono::duration_cast<std::chrono::microseconds>(search_end - search_start).count();

    // Format header
    output << "Documentation search for: \"" << query << "\"";
    output << " [mode: " << mode << "]";
    if (kind_filter) {
        output << " (kind: " << doc::doc_item_kind_to_string(*kind_filter) << ")";
    }
    if (!module_filter.empty()) {
        output << " (module: " << module_filter << ")";
    }
    if (used_expansion) {
        output << " (expanded to " << expanded_queries.size() << " queries)";
    }
    output << "\n";
    output << "Index: " << g_doc_cache.all_items.size() << " items, BM25 + HNSW";
    if (g_doc_cache.hnsw) {
        output << " (" << g_doc_cache.hnsw->dims() << "-dim vectors)";
    }
    if (g_doc_cache.build_time_ms > 0) {
        output << " [built in " << g_doc_cache.build_time_ms << "ms]";
    }
    output << " [query: " << std::fixed << std::setprecision(1) << (search_ms / 1000.0) << "ms]";
    output << "\n\n";

    if (results.empty()) {
        output << "No results found.\n\n";
        output << "Tips:\n";
        output << "- Search by name: \"split\", \"Maybe\", \"fnv1a64\"\n";
        output << "- Filter by kind: kind=\"function\", kind=\"struct\"\n";
        output << "- Filter by module: module=\"core::str\", module=\"std::json\"\n";
        output << "- Use mode=\"semantic\" for intent-based search\n";
        output << "- Use mode=\"text\" for exact keyword search\n";
    } else {
        for (const auto& result : results) {
            format_result(output, result);
        }
        output << "(" << results.size() << " result(s) found)\n";
    }

    return ToolResult::text(output.str());
}

// ============================================================================
// Documentation Get/List/Resolve Tools
// ============================================================================

auto make_docs_get_tool() -> Tool {
    return Tool{.name = "docs/get",
                .description = "Get full documentation for an item by its qualified path",
                .parameters = {
                    {"id", "string", "Fully qualified item path (e.g. core::str::split)", true},
                }};
}

auto make_docs_list_tool() -> Tool {
    return Tool{
        .name = "docs/list",
        .description = "List all documentation items in a module",
        .parameters = {
            {"module", "string", "Module path (e.g. core::str, std::json)", true},
            {"kind", "string",
             "Filter by item kind: function, method, struct, enum, behavior, constant", false},
        }};
}

auto make_docs_resolve_tool() -> Tool {
    return Tool{.name = "docs/resolve",
                .description = "Resolve a short name to its fully qualified path(s)",
                .parameters = {
                    {"name", "string", "Short name to resolve (e.g. HashMap, split)", true},
                    {"limit", "number", "Maximum results (default: 5)", false},
                }};
}

/// Formats a full documentation view for a single item (used by docs/get).
/// Shows all fields: full doc text, params, returns, examples, children, etc.
static void format_full_item(std::stringstream& out, const doc::DocItem& item,
                             const std::string& module_path) {
    auto kind_str = doc::doc_item_kind_to_string(item.kind);
    auto vis_str = doc::doc_visibility_to_string(item.visibility);

    out << "# " << module_path << "::" << item.name << "\n\n";
    out << "Kind:       " << kind_str << "\n";
    out << "Visibility: " << vis_str << "\n";
    out << "Module:     " << module_path << "\n";

    if (!item.source_file.empty()) {
        out << "Source:     " << item.source_file;
        if (item.source_line > 0)
            out << ":" << item.source_line;
        out << "\n";
    }

    if (!item.signature.empty()) {
        out << "\n```tml\n" << item.signature << "\n```\n";
    }

    // Full documentation text
    if (!item.doc.empty()) {
        out << "\n" << item.doc << "\n";
    }

    // Parameters
    if (!item.params.empty()) {
        out << "\n## Parameters\n\n";
        for (const auto& p : item.params) {
            if (p.name == "this")
                continue;
            out << "- **" << p.name << "**";
            if (!p.type.empty())
                out << ": `" << p.type << "`";
            if (!p.description.empty())
                out << " - " << p.description;
            out << "\n";
        }
    }

    // Returns
    if (item.returns) {
        out << "\n## Returns\n\n";
        if (!item.returns->type.empty())
            out << "Type: `" << item.returns->type << "`\n";
        if (!item.returns->description.empty())
            out << item.returns->description << "\n";
    }

    // Throws
    if (!item.throws.empty()) {
        out << "\n## Throws\n\n";
        for (const auto& t : item.throws) {
            out << "- **" << t.error_type << "**";
            if (!t.description.empty())
                out << " - " << t.description;
            out << "\n";
        }
    }

    // Examples
    if (!item.examples.empty()) {
        out << "\n## Examples\n\n";
        for (const auto& ex : item.examples) {
            if (!ex.description.empty())
                out << ex.description << "\n\n";
            out << "```" << (ex.language.empty() ? "tml" : ex.language) << "\n";
            out << ex.code << "\n```\n\n";
        }
    }

    // Deprecation
    if (item.deprecated) {
        out << "\n## Deprecated\n\n";
        out << item.deprecated->message << "\n";
        if (!item.deprecated->since.empty())
            out << "Since: " << item.deprecated->since << "\n";
        if (!item.deprecated->replacement.empty())
            out << "Use instead: " << item.deprecated->replacement << "\n";
    }

    // Generic parameters
    if (!item.generics.empty()) {
        out << "\n## Type Parameters\n\n";
        for (const auto& g : item.generics) {
            out << "- **" << g.name << "**";
            if (!g.bounds.empty()) {
                out << ": ";
                for (size_t i = 0; i < g.bounds.size(); ++i) {
                    if (i > 0)
                        out << " + ";
                    out << g.bounds[i];
                }
            }
            if (g.default_value)
                out << " = " << *g.default_value;
            out << "\n";
        }
    }

    // Fields (for structs)
    if (!item.fields.empty()) {
        out << "\n## Fields\n\n";
        for (const auto& f : item.fields) {
            out << "- **" << f.name << "**";
            if (!f.signature.empty())
                out << ": `" << f.signature << "`";
            if (!f.summary.empty())
                out << " - " << f.summary;
            out << "\n";
        }
    }

    // Variants (for enums)
    if (!item.variants.empty()) {
        out << "\n## Variants\n\n";
        for (const auto& v : item.variants) {
            out << "- **" << v.name << "**";
            if (!v.signature.empty())
                out << "(" << v.signature << ")";
            if (!v.summary.empty())
                out << " - " << v.summary;
            out << "\n";
        }
    }

    // Methods
    if (!item.methods.empty()) {
        out << "\n## Methods\n\n";
        for (const auto& m : item.methods) {
            out << "- `" << m.signature << "`";
            if (!m.summary.empty())
                out << " - " << m.summary;
            out << "\n";
        }
    }

    // Super traits (for behaviors)
    if (!item.super_traits.empty()) {
        out << "\n## Super Traits\n\n";
        for (const auto& t : item.super_traits) {
            out << "- " << t << "\n";
        }
    }

    // Associated types
    if (!item.associated_types.empty()) {
        out << "\n## Associated Types\n\n";
        for (const auto& at : item.associated_types) {
            out << "- **" << at.name << "**";
            if (!at.summary.empty())
                out << " - " << at.summary;
            out << "\n";
        }
    }

    // See also
    if (!item.see_also.empty()) {
        out << "\n## See Also\n\n";
        for (const auto& s : item.see_also) {
            out << "- " << s << "\n";
        }
    }

    // Since
    if (item.since) {
        out << "\nSince: " << *item.since << "\n";
    }
}

auto handle_docs_get(const json::JsonValue& params) -> ToolResult {
    auto* id_param = params.get("id");
    if (id_param == nullptr || !id_param->is_string()) {
        return ToolResult::error("Missing or invalid 'id' parameter");
    }
    std::string id = id_param->as_string();

    ensure_doc_index();
    if (!g_doc_cache.initialized) {
        return ToolResult::error("Documentation index not available");
    }

    // Search for item by qualified path (exact match preferred)
    const doc::DocItem* best_match = nullptr;
    std::string best_mod_path;
    int best_priority = 0; // higher = better match

    for (const auto& [item, mod_path] : g_doc_cache.all_items) {
        std::string qualified = mod_path + "::" + item->name;

        if (qualified == id) {
            // Exact qualified match — best possible
            best_match = item;
            best_mod_path = mod_path;
            break;
        }
        if (item->path == id && best_priority < 3) {
            best_match = item;
            best_mod_path = mod_path;
            best_priority = 3;
        }
        if (item->name == id && best_priority < 1) {
            best_match = item;
            best_mod_path = mod_path;
            best_priority = 1;
        }
    }

    if (best_match != nullptr) {
        std::stringstream out;
        format_full_item(out, *best_match, best_mod_path);
        return ToolResult::text(out.str());
    }

    return ToolResult::text("Item not found: " + id +
                            "\n\nTip: Use docs/search to find the correct qualified name.");
}

auto handle_docs_list(const json::JsonValue& params) -> ToolResult {
    auto* module_param = params.get("module");
    if (module_param == nullptr || !module_param->is_string()) {
        return ToolResult::error("Missing or invalid 'module' parameter");
    }
    std::string module_path = module_param->as_string();

    std::optional<doc::DocItemKind> kind_filter;
    auto* kind_param = params.get("kind");
    if (kind_param != nullptr && kind_param->is_string()) {
        kind_filter = parse_kind_filter(kind_param->as_string());
    }

    ensure_doc_index();
    if (!g_doc_cache.initialized) {
        return ToolResult::error("Documentation index not available");
    }

    // Group items by kind for organized output
    std::map<doc::DocItemKind, std::vector<const doc::DocItem*>> by_kind;
    int total = 0;

    for (const auto& [item, mod_path] : g_doc_cache.all_items) {
        if (!icontains(mod_path, module_path))
            continue;
        if (kind_filter && item->kind != *kind_filter)
            continue;
        by_kind[item->kind].push_back(item);
        ++total;
    }

    std::stringstream out;
    out << "# Module: " << module_path << "\n\n";

    if (total == 0) {
        out << "No items found in module '" << module_path << "'.\n";
        out << "\nAvailable modules: core, core::str, core::num, core::slice, "
               "core::iter, core::cmp, core::fmt, std::json, std::hash, "
               "std::collections, std::os, std::crypto, std::search, ...\n";
        return ToolResult::text(out.str());
    }

    // Display order for item kinds
    const doc::DocItemKind kind_order[] = {
        doc::DocItemKind::Struct,    doc::DocItemKind::Enum,     doc::DocItemKind::Trait,
        doc::DocItemKind::TypeAlias, doc::DocItemKind::Function, doc::DocItemKind::Method,
        doc::DocItemKind::Constant,  doc::DocItemKind::Impl,     doc::DocItemKind::TraitImpl};

    for (auto kind : kind_order) {
        auto it = by_kind.find(kind);
        if (it == by_kind.end())
            continue;

        auto kind_str = doc::doc_item_kind_to_string(kind);
        out << "## " << kind_str << "s (" << it->second.size() << ")\n\n";

        for (const auto* item : it->second) {
            auto vis_str = doc::doc_visibility_to_string(item->visibility);
            out << "  " << vis_str << " " << item->name;
            if (!item->signature.empty()) {
                out << " — " << item->signature;
            }
            out << "\n";
            if (!item->summary.empty()) {
                out << "    " << item->summary << "\n";
            }
        }
        out << "\n";
    }

    out << "(" << total << " item(s) total)\n";
    out << "\nUse docs/get with a qualified name for full documentation.\n";
    return ToolResult::text(out.str());
}

auto handle_docs_resolve(const json::JsonValue& params) -> ToolResult {
    auto* name_param = params.get("name");
    if (name_param == nullptr || !name_param->is_string()) {
        return ToolResult::error("Missing or invalid 'name' parameter");
    }
    std::string name = name_param->as_string();

    int64_t limit = 5;
    auto* limit_param = params.get("limit");
    if (limit_param != nullptr && limit_param->is_integer()) {
        limit = limit_param->as_i64();
    }

    ensure_doc_index();
    if (!g_doc_cache.initialized) {
        return ToolResult::error("Documentation index not available");
    }

    std::stringstream out;
    out << "Resolving: " << name << "\n\n";

    int count = 0;
    for (const auto& [item, mod_path] : g_doc_cache.all_items) {
        if (count >= limit)
            break;
        if (!icontains(item->name, name))
            continue;

        auto kind_str = doc::doc_item_kind_to_string(item->kind);
        out << "  " << mod_path << "::" << item->name << " (" << kind_str << ")\n";
        if (!item->summary.empty()) {
            out << "    " << item->summary << "\n";
        }
        count++;
    }

    if (count == 0) {
        out << "No items found matching: " << name << "\n";
    } else {
        out << "\n(" << count << " match(es))\n";
    }

    return ToolResult::text(out.str());
}

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

    // Execute the build with 300s timeout
    auto start = std::chrono::steady_clock::now();
    auto [output, exit_code] = execute_command(cmd.str(), 300);
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
        result << "\n--- Build Output ---\n" << output;
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

    // Extract high-level stats
    std::stringstream result;
    result << "=== TML Library Coverage Report ===\n\n";

    auto get_int = [&](const char* key) -> int {
        auto* v = data.get(key);
        return (v && v->is_number()) ? static_cast<int>(v->as_i64()) : 0;
    };
    auto get_double = [&](const char* key) -> double {
        auto* v = data.get(key);
        return (v && v->is_number()) ? v->as_f64() : 0.0;
    };

    int lib_funcs = get_int("library_functions");
    int lib_covered = get_int("library_covered");
    double lib_pct = get_double("library_coverage_percent");
    int total_called = get_int("total_functions_called");
    int tests_passed = get_int("tests_passed");
    int test_files = get_int("test_files");
    int duration_ms = get_int("duration_ms");
    int mods_100 = get_int("modules_100_percent");
    int mods_partial = get_int("modules_partial");
    int mods_zero = get_int("modules_zero_coverage");

    result << std::fixed << std::setprecision(1);
    result << "Library Coverage: " << lib_covered << "/" << lib_funcs << " functions (" << lib_pct
           << "%)\n";
    result << "Total Functions Called: " << total_called << "\n";
    result << "Tests: " << tests_passed << " passed across " << test_files << " files\n";
    result << "Duration: " << duration_ms << "ms\n";
    result << "Modules: " << mods_100 << " at 100%, " << mods_partial << " partial, " << mods_zero
           << " at 0%\n\n";

    // Suite breakdown
    auto* suites = data.get("suites");
    if (suites != nullptr && suites->is_array()) {
        result << "--- Test Suites ---\n";
        const auto& suites_arr = suites->as_array();
        for (size_t si = 0; si < suites_arr.size(); ++si) {
            const auto& suite = suites_arr[si];
            const json::JsonValue* sname = suite.get("name");
            const json::JsonValue* stests = suite.get("tests");
            const json::JsonValue* sdur = suite.get("duration_ms");
            if (sname != nullptr && sname->is_string()) {
                result << "  " << sname->as_string();
                if (stests != nullptr && stests->is_number()) {
                    result << ": " << static_cast<int>(stests->as_i64()) << " tests";
                }
                if (sdur != nullptr && sdur->is_number()) {
                    result << " (" << static_cast<int>(sdur->as_i64()) << "ms)";
                }
                result << "\n";
            }
        }
        result << "\n";
    }

    // Now scan library source files to build per-module coverage breakdown
    // by cross-referencing non_library_functions with source file function declarations
    auto* non_lib_funcs = data.get("non_library_functions");
    if (non_lib_funcs != nullptr && non_lib_funcs->is_array()) {
        // Build a set of all called functions for quick lookup
        std::set<std::string> called_functions;
        const auto& func_arr = non_lib_funcs->as_array();
        for (size_t fi = 0; fi < func_arr.size(); ++fi) {
            if (func_arr[fi].is_string()) {
                called_functions.insert(func_arr[fi].as_string());
            }
        }

        // Scan library source files to build per-module function lists
        struct ModuleInfo {
            std::string name;
            std::vector<std::string> all_functions;
            std::vector<std::string> covered;
            std::vector<std::string> uncovered;
        };

        std::map<std::string, ModuleInfo> module_map;

        // Dynamically scan all lib/ subdirectories (core, std, test, backtrace, etc.)
        std::vector<fs::path> lib_dirs;
        auto lib_root = root / "lib";
        if (fs::exists(lib_root) && fs::is_directory(lib_root)) {
            for (const auto& entry : fs::directory_iterator(lib_root)) {
                if (entry.is_directory()) {
                    auto src_dir = entry.path() / "src";
                    if (fs::exists(src_dir)) {
                        lib_dirs.push_back(src_dir);
                    }
                }
            }
        }

        for (const auto& lib_dir : lib_dirs) {
            if (!fs::exists(lib_dir))
                continue;

            for (const auto& entry : fs::recursive_directory_iterator(lib_dir)) {
                if (!entry.is_regular_file())
                    continue;
                if (entry.path().extension() != ".tml")
                    continue;

                // Extract module name from path
                auto rel = fs::relative(entry.path(), root / "lib");
                std::string mod_name = rel.parent_path().string();
                // Normalize separators
                std::replace(mod_name.begin(), mod_name.end(), '\\', '/');

                // Remove "<libname>/src/" prefix to get clean module name
                // e.g. "core/src/str" -> "str", "test/src" -> "test"
                auto slash_pos = mod_name.find('/');
                if (slash_pos != std::string::npos) {
                    std::string lib_name = mod_name.substr(0, slash_pos);
                    std::string rest = mod_name.substr(slash_pos + 1);
                    // Remove "src/" prefix from rest
                    if (rest.find("src/") == 0) {
                        mod_name = lib_name + "/" + rest.substr(4);
                    } else if (rest == "src") {
                        mod_name = lib_name;
                    }
                }

                if (mod_name.empty())
                    continue;

                // Read file and extract function/method names
                std::ifstream src_file(entry.path());
                if (!src_file.is_open())
                    continue;

                std::string line;
                std::string current_type;
                while (std::getline(src_file, line)) {
                    // Track impl blocks: "impl Type {" or "impl Behavior for Type {"
                    if (line.find("impl ") != std::string::npos) {
                        // Extract the type name
                        auto pos = line.find("impl ");
                        auto rest = line.substr(pos + 5);
                        // Handle "impl Behavior for Type {"
                        auto for_pos = rest.find(" for ");
                        if (for_pos != std::string::npos) {
                            rest = rest.substr(for_pos + 5);
                        }
                        // Extract just the type name (before [ or { or space)
                        std::string type_name;
                        for (char c : rest) {
                            if (c == '[' || c == '{' || c == ' ' || c == '(')
                                break;
                            type_name += c;
                        }
                        if (!type_name.empty()) {
                            current_type = type_name;
                        }
                    }

                    // Track "func name(" declarations
                    auto func_pos = line.find("func ");
                    if (func_pos != std::string::npos) {
                        auto rest = line.substr(func_pos + 5);
                        std::string func_name;
                        for (char c : rest) {
                            if (c == '(' || c == '[' || c == ' ' || c == '<')
                                break;
                            func_name += c;
                        }
                        if (!func_name.empty()) {
                            std::string qualified_name;
                            if (!current_type.empty()) {
                                qualified_name = current_type + "::" + func_name;
                            } else {
                                qualified_name = func_name;
                            }
                            module_map[mod_name].name = mod_name;
                            module_map[mod_name].all_functions.push_back(qualified_name);

                            if (called_functions.count(qualified_name) > 0) {
                                module_map[mod_name].covered.push_back(qualified_name);
                            } else {
                                module_map[mod_name].uncovered.push_back(qualified_name);
                            }
                        }
                    }

                    // Reset current_type at end of impl block (simple heuristic)
                    // Only reset when we see a top-level closing brace
                    // This is approximate but good enough for coverage analysis
                }
            }
        }

        // Apply module filter if specified
        auto* module_filter = params.get("module");
        std::string filter_str;
        if (module_filter != nullptr && module_filter->is_string()) {
            filter_str = module_filter->as_string();
            // Normalize :: to /
            std::string normalized = filter_str;
            size_t pos = 0;
            while ((pos = normalized.find("::", pos)) != std::string::npos) {
                normalized.replace(pos, 2, "/");
            }
            filter_str = normalized;
        }

        // Build sorted module list
        std::vector<ModuleInfo> sorted_modules;
        for (auto& [name, info] : module_map) {
            if (info.all_functions.empty())
                continue;
            if (!filter_str.empty() && name.find(filter_str) == std::string::npos)
                continue;
            sorted_modules.push_back(std::move(info));
        }

        // Sort
        std::string sort_order = "lowest";
        auto* sort_param = params.get("sort");
        if (sort_param != nullptr && sort_param->is_string()) {
            sort_order = sort_param->as_string();
        }

        if (sort_order == "name") {
            std::sort(sorted_modules.begin(), sorted_modules.end(),
                      [](const auto& a, const auto& b) { return a.name < b.name; });
        } else if (sort_order == "highest") {
            std::sort(sorted_modules.begin(), sorted_modules.end(),
                      [](const auto& a, const auto& b) {
                          double pct_a = a.all_functions.empty()
                                             ? 0
                                             : (100.0 * a.covered.size() / a.all_functions.size());
                          double pct_b = b.all_functions.empty()
                                             ? 0
                                             : (100.0 * b.covered.size() / b.all_functions.size());
                          return pct_a > pct_b;
                      });
        } else { // lowest (default)
            std::sort(sorted_modules.begin(), sorted_modules.end(),
                      [](const auto& a, const auto& b) {
                          double pct_a = a.all_functions.empty()
                                             ? 0
                                             : (100.0 * a.covered.size() / a.all_functions.size());
                          double pct_b = b.all_functions.empty()
                                             ? 0
                                             : (100.0 * b.covered.size() / b.all_functions.size());
                          return pct_a < pct_b;
                      });
        }

        // Apply limit
        int limit = static_cast<int>(sorted_modules.size());
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

        for (int i = 0; i < limit && i < static_cast<int>(sorted_modules.size()); ++i) {
            const auto& mod = sorted_modules[i];
            double pct = mod.all_functions.empty()
                             ? 0.0
                             : (100.0 * mod.covered.size() / mod.all_functions.size());
            result << std::left << std::setw(30) << mod.name << std::right << std::setw(10)
                   << mod.covered.size() << std::setw(10) << mod.all_functions.size()
                   << std::setw(9) << std::fixed << std::setprecision(1) << pct << "%\n";
        }

        if (limit < static_cast<int>(sorted_modules.size())) {
            result << "... and " << (sorted_modules.size() - limit) << " more modules\n";
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

} // namespace tml::mcp
