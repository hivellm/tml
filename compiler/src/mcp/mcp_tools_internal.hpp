//! # MCP Tools Internal Helpers
//!
//! Shared utilities used across the split mcp_tools_*.cpp files.
//! This is an internal header — not part of the public MCP API.

#pragma once

#include "doc/doc_model.hpp"
#include "mcp/mcp_server.hpp"
#include "mcp/mcp_tools.hpp"

#include "json/json_parser.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace tml::mcp {

namespace fs = std::filesystem;

// ============================================================================
// Shared Helper Functions
// ============================================================================

/// Strips ANSI escape sequences from text output.
auto strip_ansi(const std::string& input) -> std::string;

/// Reads a file and returns its contents, or nullopt on error.
auto read_source_file(const std::string& path) -> std::optional<std::string>;

/// Executes a command and returns its output (ANSI-stripped) and exit code.
///
/// When `use_daemon` is true the child is launched with `TML_DAEMON=1` in its
/// environment so the thin launcher forwards `check`/`build`/`run` to the warm
/// compile daemon (~8 ms cache-hit vs ~450 ms cold start), and the daemon is
/// auto-started (detached, throttled) if it is not running. Only pass true for
/// commands whose output is a pure function of the source files (check, build,
/// emit-ir, emit-mir) — never for `run` or `test`.
auto execute_command(const std::string& cmd, int timeout_seconds = 120, bool use_daemon = false)
    -> std::pair<std::string, int>;

/// Gets the path to the TML compiler executable.
auto get_tml_executable() -> std::string;

// ============================================================================
// Doc Cache Access (defined in mcp_tools_docs.cpp)
// ============================================================================

/// Ensures the doc index is built and up-to-date.
void ensure_doc_index();

/// Returns the flat list of all doc items (doc_id -> DocItem*, module_path).
auto get_doc_all_items() -> const std::vector<std::pair<const tml::doc::DocItem*, std::string>>&;

/// Returns whether the doc cache is initialized.
auto is_doc_cache_initialized() -> bool;

/// Case-insensitive substring search.
auto icontains(const std::string& haystack, const std::string& needle) -> bool;

/// Converts a string to a DocItemKind filter, or nullopt if invalid.
auto parse_kind_filter(const std::string& kind) -> std::optional<tml::doc::DocItemKind>;

/// Discovers the TML project root by walking up from cwd or executable location.
auto find_tml_root() -> fs::path;

// ============================================================================
// Docs Hint Helpers (mcp_tools.cpp)
// ============================================================================

/// Extracts candidate type/identifier names from an error string.
/// Finds single-quoted tokens and qualified module paths (containing "::").
auto extract_hint_candidates(const std::string& text) -> std::vector<std::string>;

/// Builds docs hint lines from extracted candidates (max 5 hints).
/// Returns empty string if no useful hints are found.
auto build_docs_hints(const std::string& error_text) -> std::string;

/// Parses a TML source file for top-level `use` statements and returns
/// the module prefix of each import.
auto extract_imports(const std::string& file_path) -> std::vector<std::string>;

// ============================================================================
// Codegen Handlers (mcp_tools_codegen.cpp)
// ============================================================================

/// Emits LLVM IR for a TML source file.
auto handle_emit_ir(const json::JsonValue& params) -> ToolResult;

/// Emits MIR for a TML source file.
auto handle_emit_mir(const json::JsonValue& params) -> ToolResult;

/// Runs TML tests.
auto handle_test(const json::JsonValue& params) -> ToolResult;

/// Formats a TML source file.
auto handle_format(const json::JsonValue& params) -> ToolResult;

/// Lints a TML source file.
auto handle_lint(const json::JsonValue& params) -> ToolResult;

// ============================================================================
// Debug/Inspector Tools (mcp_tools_debug.cpp)
// ============================================================================

/// Creates the inspect tool definition.
auto make_inspect_tool() -> Tool;

/// Handles the inspect tool request.
auto handle_inspect(const json::JsonValue& params) -> ToolResult;

/// Creates the profile tool definition.
auto make_profile_tool() -> Tool;

/// Handles the profile tool request.
auto handle_profile(const json::JsonValue& params) -> ToolResult;

/// Creates the debug tool definition.
auto make_debug_tool() -> Tool;

/// Handles the debug tool request.
auto handle_debug(const json::JsonValue& params) -> ToolResult;

} // namespace tml::mcp