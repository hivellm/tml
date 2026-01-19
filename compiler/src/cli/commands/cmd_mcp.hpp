//! # MCP Command Interface
//!
//! This header defines the MCP (Model Context Protocol) server command.
//!
//! ## Usage
//!
//! ```bash
//! tml mcp           # Start MCP server
//! tml mcp --verbose # Start with verbose logging
//! ```
//!
//! ## Protocol
//!
//! The server uses JSON-RPC 2.0 over stdio transport.

#pragma once

#include <string>
#include <vector>

namespace tml::cli {

/// Runs the MCP server command.
///
/// # Arguments
///
/// * `args` - Command line arguments after `tml mcp`
///
/// # Returns
///
/// Exit code (0 for success, non-zero for error).
auto cmd_mcp(const std::vector<std::string>& args) -> int;

} // namespace tml::cli
