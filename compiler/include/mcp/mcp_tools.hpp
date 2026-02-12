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
//! | `docs/search` | Search documentation (BM25 + HNSW hybrid) |
//! | `docs/get` | Get full documentation for an item |
//! | `docs/list` | List items in a module |
//! | `docs/resolve` | Resolve item by qualified path |
//! | `cache/invalidate` | Invalidate cache for specific files |
//! | `project/build` | Build the TML compiler from C++ sources |
//! | `project/coverage` | Read structured coverage data from last test run |
//! | `project/structure` | Show project module tree with file counts |
//! | `project/affected-tests` | Detect tests affected by recent changes |
//! | `project/artifacts` | List build artifacts with size and age |
//! | `explain` | Show detailed error code explanation |
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
/// - `docs/search` - Documentation search (BM25 + HNSW hybrid)
/// - `docs/get` - Full item documentation
/// - `docs/list` - List module items
/// - `docs/resolve` - Resolve item by path
/// - `cache/invalidate` - Cache invalidation
/// - `project/build` - Build TML compiler from C++ sources
/// - `project/coverage` - Structured coverage report from last test run
/// - `explain` - Error code explanation
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
/// - `no_cache` (boolean, optional): Force full recompilation (disable test cache)
/// - `fail_fast` (boolean, optional): Stop on first test failure
/// - `structured` (boolean, optional): Return parsed results: total, passed, failed, failures[]
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
/// Searches the TML documentation index built from library sources.
/// Uses AST-based extraction with scored relevance ranking.
///
/// Parameters:
/// - `query` (string, required): Search query (matches names, signatures, docs)
/// - `limit` (number, optional): Maximum results (default: 10)
/// - `kind` (string, optional): Filter by item kind (function, method, struct, enum, behavior,
///   constant, field, variant)
/// - `module` (string, optional): Filter by module path (e.g. "core::str", "std::json")
/// - `mode` (string, optional): Search mode — "text" (BM25), "semantic" (HNSW), "hybrid" (default)
auto make_docs_search_tool() -> Tool;

/// Returns the `docs/get` tool definition.
///
/// Gets full documentation for an item by its qualified path.
///
/// Parameters:
/// - `id` (string, required): Fully qualified item path (e.g. "core::str::split")
auto make_docs_get_tool() -> Tool;

/// Returns the `docs/list` tool definition.
///
/// Lists all documentation items in a module.
///
/// Parameters:
/// - `module` (string, required): Module path (e.g. "core::str", "std::json")
/// - `kind` (string, optional): Filter by item kind
auto make_docs_list_tool() -> Tool;

/// Returns the `docs/resolve` tool definition.
///
/// Resolves a short name to its fully qualified path(s).
///
/// Parameters:
/// - `name` (string, required): Short name to resolve (e.g. "HashMap", "split")
/// - `limit` (number, optional): Maximum results (default: 5)
auto make_docs_resolve_tool() -> Tool;

/// Returns the `cache/invalidate` tool definition.
///
/// Parameters:
/// - `files` (array, required): List of file paths to invalidate
/// - `verbose` (boolean, optional): Show detailed output
auto make_cache_invalidate_tool() -> Tool;

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
/// Uses BM25 text index, HNSW vector index, or hybrid mode with
/// reciprocal rank fusion for ranked documentation search.
///
/// # Arguments
///
/// * `params` - Tool parameters (query, limit, kind, module, mode)
///
/// # Returns
///
/// Result with ranked search results.
auto handle_docs_search(const json::JsonValue& params) -> ToolResult;

/// Handles the `docs/get` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (id)
///
/// # Returns
///
/// Result with full documentation for the item.
auto handle_docs_get(const json::JsonValue& params) -> ToolResult;

/// Handles the `docs/list` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (module, kind)
///
/// # Returns
///
/// Result with items in the specified module.
auto handle_docs_list(const json::JsonValue& params) -> ToolResult;

/// Handles the `docs/resolve` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (name, limit)
///
/// # Returns
///
/// Result with matching fully qualified paths.
auto handle_docs_resolve(const json::JsonValue& params) -> ToolResult;

/// Handles the `cache/invalidate` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (files, verbose)
///
/// # Returns
///
/// Result with invalidation status.
auto handle_cache_invalidate(const json::JsonValue& params) -> ToolResult;

/// Returns the `project/build` tool definition.
///
/// Builds the TML compiler from C++ sources using the project build scripts.
/// Eliminates the need for complex shell commands with path escaping.
///
/// Parameters:
/// - `mode` (string, optional): Build mode — "debug" (default) or "release"
/// - `clean` (boolean, optional): Clean build directory first
/// - `tests` (boolean, optional): Build test executable (default: true)
/// - `target` (string, optional): Build target — "all" (default), "compiler" (tml.exe only),
///   "mcp" (tml_mcp.exe only). Use "compiler" to update tml.exe without rebuilding the MCP server.
auto make_project_build_tool() -> Tool;

/// Handles the `project/build` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (mode, clean, tests)
///
/// # Returns
///
/// Result with build status, duration, and output path.
auto handle_project_build(const json::JsonValue& params) -> ToolResult;

/// Returns the `project/coverage` tool definition.
///
/// Reads and returns structured coverage data from the last test run.
/// Parses build/coverage/coverage.json with optional per-module breakdown.
///
/// Parameters:
/// - `module` (string, optional): Filter to specific module (e.g., "core::str")
/// - `sort` (string, optional): Sort order — "lowest" (default), "name", "highest"
/// - `limit` (number, optional): Maximum modules to return
/// - `refresh` (boolean, optional): Run tests with --coverage first
auto make_project_coverage_tool() -> Tool;

/// Handles the `project/coverage` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (module, sort, limit, refresh)
///
/// # Returns
///
/// Result with coverage statistics and per-module breakdown.
auto handle_project_coverage(const json::JsonValue& params) -> ToolResult;

/// Returns the `explain` tool definition.
///
/// Shows detailed explanation for a TML compiler error code.
///
/// Parameters:
/// - `code` (string, required): Error code (e.g., "T001", "B001", "L003")
auto make_explain_tool() -> Tool;

/// Handles the `explain` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (code)
///
/// # Returns
///
/// Result with error description, common causes, and fix examples.
auto handle_explain(const json::JsonValue& params) -> ToolResult;

/// Returns the `project/structure` tool definition.
///
/// Shows the TML project module tree with file counts and test coverage.
/// Uses std::filesystem for directory enumeration without shell commands.
///
/// Parameters:
/// - `module` (string, optional): Filter to specific library or module (e.g., "core", "std::json")
/// - `depth` (number, optional): Maximum directory depth to display (default: 3)
/// - `show_files` (boolean, optional): Show individual file names instead of just counts
auto make_project_structure_tool() -> Tool;

/// Handles the `project/structure` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (module, depth, show_files)
///
/// # Returns
///
/// Result with module tree showing file counts and structure.
auto handle_project_structure(const json::JsonValue& params) -> ToolResult;

/// Returns the `project/affected-tests` tool definition.
///
/// Detects which test files are affected by recent changes using git diff.
/// Maps changed source files to their corresponding test directories.
///
/// Parameters:
/// - `base` (string, optional): Git ref to diff against (default: "HEAD")
/// - `run` (boolean, optional): Automatically run the affected tests
/// - `verbose` (boolean, optional): Show detailed mapping of changes to tests
auto make_project_affected_tests_tool() -> Tool;

/// Handles the `project/affected-tests` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (base, run, verbose)
///
/// # Returns
///
/// Result with affected test directories and optionally test results.
auto handle_project_affected_tests(const json::JsonValue& params) -> ToolResult;

/// Returns the `project/artifacts` tool definition.
///
/// Lists build artifacts: executables, libraries, cache directories,
/// and coverage files with size and modification time.
///
/// Parameters:
/// - `kind` (string, optional): Filter: "executables", "libraries", "cache", "coverage", "all"
/// - `config` (string, optional): Build config: "debug", "release", "all" (default: "debug")
auto make_project_artifacts_tool() -> Tool;

/// Handles the `project/artifacts` tool invocation.
///
/// # Arguments
///
/// * `params` - Tool parameters (kind, config)
///
/// # Returns
///
/// Result with artifact listing including sizes and ages.
auto handle_project_artifacts(const json::JsonValue& params) -> ToolResult;

} // namespace tml::mcp
