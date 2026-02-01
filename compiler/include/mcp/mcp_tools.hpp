//! # MCP Compiler Tools
//!
//! Tool definitions and handlers for the TML compiler MCP server.
//!
//! ## Available Tools
//!
//! | Tool | Description |
//! |------|-------------|
//! | `compile` | Compile a TML source file to executable |
//! | `run` | Build and execute a TML file |
//! | `build` | Build a TML file with full options |
//! | `check` | Type check without compiling |
//! | `emit-ir` | Emit LLVM IR for a source file (supports chunking) |
//! | `emit-mir` | Emit MIR for a source file |
//! | `test` | Run tests |
//! | `format` | Format TML source files |
//! | `lint` | Lint TML source files |
//! | `docs/search` | Search documentation |
//!
//! ## Usage
//!
//! ```cpp
//! #include "mcp/mcp_tools.hpp"
//! #include "mcp/mcp_server.hpp"
//!
//! mcp::McpServer server;
//! mcp::register_compiler_tools(server);
//! server.run();
//! ```

#pragma once

#include "mcp/mcp_server.hpp"

namespace tml::mcp {

/// Registers all compiler tools with the MCP server.
///
/// This includes:
/// - `compile` - Full compilation to executable
/// - `run` - Build and execute a TML file
/// - `build` - Build with full options
/// - `check` - Type checking only
/// - `emit-ir` - LLVM IR emission (with chunking support)
/// - `emit-mir` - MIR emission
/// - `test` - Test runner
/// - `format` - Code formatting
/// - `lint` - Code linting
/// - `docs/search` - Documentation search
///
/// # Arguments
///
/// * `server` - The MCP server to register tools with
void register_compiler_tools(McpServer& server);

// ============================================================================
// Tool Definitions
// ============================================================================

/// Returns the `compile` tool definition.
///
/// Parameters:
/// - `file` (string, required): Path to the source file
/// - `output` (string, optional): Output file path
/// - `optimize` (string, optional): Optimization level (O0, O1, O2, O3)
/// - `release` (boolean, optional): Build in release mode
auto make_compile_tool() -> Tool;

/// Returns the `check` tool definition.
///
/// Parameters:
/// - `file` (string, required): Path to the source file
auto make_check_tool() -> Tool;

/// Returns the `run` tool definition.
///
/// Parameters:
/// - `file` (string, required): Path to the source file
/// - `args` (array, optional): Arguments to pass to the program
/// - `release` (boolean, optional): Build in release mode
auto make_run_tool() -> Tool;

/// Returns the `build` tool definition (full build with output).
///
/// Parameters:
/// - `file` (string, required): Path to the source file
/// - `output` (string, optional): Output file path
/// - `optimize` (string, optional): Optimization level (O0, O1, O2, O3)
/// - `release` (boolean, optional): Build in release mode
/// - `crate_type` (string, optional): Output type (bin, lib, dylib, rlib)
auto make_build_tool() -> Tool;

/// Returns the `emit-ir` tool definition.
///
/// Parameters:
/// - `file` (string, required): Path to the source file
/// - `optimize` (string, optional): Optimization level (O0, O1, O2, O3)
/// - `function` (string, optional): Filter to specific function name
/// - `offset` (number, optional): Line offset for chunked output
/// - `limit` (number, optional): Max lines to return (default: all)
auto make_emit_ir_tool() -> Tool;

/// Returns the `emit-mir` tool definition.
///
/// Parameters:
/// - `file` (string, required): Path to the source file
auto make_emit_mir_tool() -> Tool;

/// Returns the `test` tool definition.
///
/// Parameters:
/// - `path` (string, optional): Path to test file or directory
/// - `filter` (string, optional): Test name filter
/// - `release` (boolean, optional): Run in release mode
/// - `coverage` (boolean, optional): Generate coverage report
/// - `profile` (boolean, optional): Show per-test timing profile
/// - `verbose` (boolean, optional): Show verbose output
auto make_test_tool() -> Tool;

/// Returns the `format` tool definition.
///
/// Parameters:
/// - `file` (string, required): Path to the source file or directory
/// - `check` (boolean, optional): Check formatting without modifying files
auto make_format_tool() -> Tool;

/// Returns the `lint` tool definition.
///
/// Parameters:
/// - `file` (string, required): Path to the source file or directory
/// - `fix` (boolean, optional): Automatically fix issues where possible
auto make_lint_tool() -> Tool;

/// Returns the `docs/search` tool definition.
///
/// Parameters:
/// - `query` (string, required): Search query
/// - `limit` (number, optional): Maximum results (default: 10)
auto make_docs_search_tool() -> Tool;

// ============================================================================
// Tool Handlers
// ============================================================================

/// Handles the `compile` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (file, output, optimize, release)
///
/// # Returns
///
/// Result with compilation status and any diagnostics.
auto handle_compile(const json::JsonValue& params) -> ToolResult;

/// Handles the `check` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (file)
///
/// # Returns
///
/// Result with type check status and diagnostics.
auto handle_check(const json::JsonValue& params) -> ToolResult;

/// Handles the `run` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (file, args, release)
///
/// # Returns
///
/// Result with program output and exit code.
auto handle_run(const json::JsonValue& params) -> ToolResult;

/// Handles the `build` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (file, output, optimize, release, crate_type)
///
/// # Returns
///
/// Result with build status and output path.
auto handle_build(const json::JsonValue& params) -> ToolResult;

/// Handles the `emit-ir` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (file, optimize, function, offset, limit)
///
/// # Returns
///
/// Result containing LLVM IR text (chunked if offset/limit specified).
auto handle_emit_ir(const json::JsonValue& params) -> ToolResult;

/// Handles the `emit-mir` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (file)
///
/// # Returns
///
/// Result containing MIR text.
auto handle_emit_mir(const json::JsonValue& params) -> ToolResult;

/// Handles the `test` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (path, filter, release)
///
/// # Returns
///
/// Result with test results summary.
auto handle_test(const json::JsonValue& params) -> ToolResult;

/// Handles the `format` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (file, check)
///
/// # Returns
///
/// Result with formatting status.
auto handle_format(const json::JsonValue& params) -> ToolResult;

/// Handles the `lint` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (file, fix)
///
/// # Returns
///
/// Result with lint diagnostics.
auto handle_lint(const json::JsonValue& params) -> ToolResult;

/// Handles the `docs/search` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (query, limit)
///
/// # Returns
///
/// Result with search results.
auto handle_docs_search(const json::JsonValue& params) -> ToolResult;

} // namespace tml::mcp
