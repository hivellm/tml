//! # MCP Tools Internal Helpers
//!
//! Shared utilities used across the split mcp_tools_*.cpp files.
//! This is an internal header — not part of the public MCP API.

#pragma once

#include "doc/doc_model.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mcp/mcp_server.hpp"
#include "mcp/mcp_tools.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "types/checker.hpp"

#include "json/json_parser.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
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
// Shared Types
// ============================================================================

struct CompileContext {
    parser::Module module;
    types::TypeEnv type_env;
};

struct CompileError {
    std::string message;
};

// ============================================================================
// Shared Helper Functions
// ============================================================================

/// Strips ANSI escape sequences from text output.
auto strip_ansi(const std::string& input) -> std::string;

/// Reads a file and returns its contents, or nullopt on error.
auto read_source_file(const std::string& path) -> std::optional<std::string>;

/// Parses and type-checks TML source code.
auto parse_and_check(const std::string& source, const std::string& filename)
    -> std::variant<CompileContext, CompileError>;

/// Executes a command and returns its output (ANSI-stripped) and exit code.
auto execute_command(const std::string& cmd, int timeout_seconds = 120)
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

} // namespace tml::mcp