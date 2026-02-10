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
#include "hir/hir_builder.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/hir_mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "types/checker.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
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
    server.register_tool(make_cache_invalidate_tool(), handle_cache_invalidate);
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
    return Tool{.name = "docs/search",
                .description = "Search TML documentation",
                .parameters = {
                    {"query", "string", "Search query", true},
                    {"limit", "number", "Maximum results (default: 10)", false},
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

static auto execute_command(const std::string& cmd) -> std::pair<std::string, int>;
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
// Helper: Execute Command and Capture Output
// ============================================================================

/// Executes a command and returns its output and exit code.
static auto execute_command(const std::string& cmd) -> std::pair<std::string, int> {
    std::string output;
    int exit_code = -1;

#ifdef _WIN32
    // Windows: Execute command directly, redirecting stderr to stdout
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = _popen(full_cmd.c_str(), "r");
    if (pipe) {
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
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
        }
        int status = pclose(pipe);
        exit_code = WEXITSTATUS(status);
    }
#endif

    return {output, exit_code};
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

    // Execute tests
    auto [output, exit_code] = execute_command(cmd.str());

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
    cmd << tml_exe << " format " << file_path;

    // Add check flag if specified
    auto* check_param = params.get("check");
    if (check_param != nullptr && check_param->is_bool() && check_param->as_bool()) {
        cmd << " --check";
    }

    // Execute
    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Format successful!\n";
        result << "Path: " << file_path << "\n";
    } else {
        result << "Format failed or found unformatted files (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    if (exit_code != 0) {
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

/// Helper to search for a query in a file and return matching lines with context.
static auto search_file_for_query(const fs::path& file_path, const std::string& query,
                                  int context_lines = 2)
    -> std::vector<std::pair<int, std::string>> {
    std::vector<std::pair<int, std::string>> results;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return results;
    }

    // Convert query to lowercase for case-insensitive search
    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line_lower = lines[i];
        std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (line_lower.find(query_lower) != std::string::npos) {
            // Build context snippet
            std::stringstream snippet;
            size_t start = (i >= static_cast<size_t>(context_lines)) ? i - context_lines : 0;
            size_t end = std::min(i + context_lines + 1, lines.size());
            for (size_t j = start; j < end; ++j) {
                if (j == i) {
                    snippet << ">>> ";
                } else {
                    snippet << "    ";
                }
                snippet << lines[j] << "\n";
            }
            results.push_back({static_cast<int>(i + 1), snippet.str()});
        }
    }
    return results;
}

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

    std::stringstream output;
    output << "Documentation search for: \"" << query << "\"\n\n";

    int results_found = 0;

    // Search paths relative to the TML executable or common locations
    std::vector<std::string> search_dirs = {
        "docs",
        "../docs",
        "../../docs",
        "F:/Node/hivellm/tml/docs",
        "lib/core/src",
        "lib/std/src",
        "../lib/core/src",
        "../lib/std/src",
        "F:/Node/hivellm/tml/lib/core/src",
        "F:/Node/hivellm/tml/lib/std/src",
    };

    for (const auto& dir : search_dirs) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) {
            continue;
        }

        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (results_found >= limit) {
                break;
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            auto ext = entry.path().extension().string();
            if (ext != ".md" && ext != ".tml") {
                continue;
            }

            auto matches = search_file_for_query(entry.path(), query);
            for (const auto& [line_num, snippet] : matches) {
                if (results_found >= limit) {
                    break;
                }
                output << "--- " << entry.path().string() << ":" << line_num << " ---\n";
                output << snippet << "\n";
                results_found++;
            }
        }
        if (results_found >= limit) {
            break;
        }
    }

    if (results_found == 0) {
        output << "No results found.\n\n";
        output << "Tip: Try searching for:\n";
        output << "- Type names: Maybe, Outcome, Text, Str\n";
        output << "- Keywords: func, struct, enum, behavior\n";
        output << "- Module names: collections, iter, slice\n";
    } else {
        output << "\n(" << results_found << " result(s) found)\n";
    }

    return ToolResult::text(output.str());
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

} // namespace tml::mcp
