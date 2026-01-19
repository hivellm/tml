//! # MCP Command
//!
//! Implements the `tml mcp` command to start the MCP server.
//!
//! ## Usage
//!
//! ```bash
//! # Start MCP server with stdio transport
//! tml mcp
//!
//! # Start with verbose logging
//! tml mcp --verbose
//! ```
//!
//! ## Protocol
//!
//! The server uses JSON-RPC 2.0 over stdio:
//! - Reads requests from stdin (newline-delimited JSON)
//! - Writes responses to stdout (newline-delimited JSON)
//! - Writes logs to stderr

#include "mcp/mcp_server.hpp"
#include "mcp/mcp_tools.hpp"

#include <iostream>
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
auto cmd_mcp(const std::vector<std::string>& args) -> int {
    bool verbose = false;

    // Parse arguments
    for (const auto& arg : args) {
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cerr << R"(
Usage: tml mcp [options]

Start the TML MCP (Model Context Protocol) server.

Options:
  --verbose, -v    Enable verbose logging
  --help, -h       Show this help message

The server uses stdio transport:
  - Reads JSON-RPC requests from stdin
  - Writes JSON-RPC responses to stdout
  - Writes logs to stderr

Available tools:
  compile     Compile a TML source file
  check       Type check without compiling
  emit-ir     Emit LLVM IR
  emit-mir    Emit MIR
  test        Run tests
  docs/search Search documentation
)";
            return 0;
        } else if (arg.starts_with("-")) {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            return 1;
        }
    }

    if (verbose) {
        std::cerr << "[MCP] Starting TML MCP server...\n";
        std::cerr << "[MCP] Transport: stdio\n";
        std::cerr << "[MCP] Protocol version: " << mcp::MCP_PROTOCOL_VERSION << "\n";
    }

    // Create and configure server
    mcp::McpServer server("tml-compiler", "0.1.0");

    // Register compiler tools
    mcp::register_compiler_tools(server);

    if (verbose) {
        std::cerr << "[MCP] Server ready, waiting for requests...\n";
    }

    // Run server (blocks until shutdown)
    server.run();

    if (verbose) {
        std::cerr << "[MCP] Server shutdown complete.\n";
    }

    return 0;
}

} // namespace tml::cli
