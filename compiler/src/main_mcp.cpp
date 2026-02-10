//! # TML MCP Server Entry Point
//!
//! This file is the main entry point for the TML MCP (Model Context Protocol) server.
//! It is a separate executable (`tml_mcp`) from the main compiler (`tml`) to avoid
//! file locking issues when the IDE extension needs the compiler while MCP is running.
//!
//! ## Why a Separate Executable?
//!
//! When Claude Code or other IDE extensions use MCP, the `tml_mcp.exe` process runs
//! continuously. If we used the main `tml.exe` for MCP, it would be locked and
//! couldn't be rebuilt during development. Having a separate executable allows:
//!
//! - Rebuilding `tml.exe` while MCP server is running
//! - Independent versioning of MCP server
//! - Cleaner process management
//!
//! ## Usage
//!
//! ```bash
//! # Start MCP server with stdio transport
//! tml_mcp
//!
//! # Start with verbose logging
//! tml_mcp --verbose
//! ```
//!
//! ## Protocol
//!
//! The server uses JSON-RPC 2.0 over stdio:
//! - Reads requests from stdin (newline-delimited JSON)
//! - Writes responses to stdout (newline-delimited JSON)
//! - Writes logs to stderr

#include "log/log.hpp"
#include "mcp/mcp_server.hpp"
#include "mcp/mcp_tools.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

/// Main entry point for the TML MCP server.
///
/// @param argc Argument count from the operating system
/// @param argv Argument vector (null-terminated strings)
/// @return Exit code: 0 for success, non-zero for errors
int main(int argc, char* argv[]) {
    bool verbose = false;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cerr << R"(
TML MCP Server - Model Context Protocol for TML Compiler

Usage: tml_mcp [options]

Options:
  --verbose, -v    Enable verbose logging to stderr
  --help, -h       Show this help message

Transport:
  - Reads JSON-RPC 2.0 requests from stdin (newline-delimited)
  - Writes JSON-RPC 2.0 responses to stdout (newline-delimited)
  - Writes logs to stderr

Available tools:
  compile           Compile a TML source file
  run               Build and execute a TML source file
  build             Build with full options
  check             Type check without compiling
  emit-ir           Emit LLVM IR (with chunking support)
  emit-mir          Emit MIR
  test              Run tests
  format            Format source files
  lint              Lint source files
  docs/search       Search documentation
  cache/invalidate  Invalidate cache for specific files

Note: This is a standalone executable separate from the main 'tml' compiler
to avoid file locking issues during development.
)";
            return 0;
        } else if (arg == "--version" || arg == "-V") {
            std::cerr << "tml_mcp 0.1.0\n";
            return 0;
        } else if (arg.starts_with("-")) {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            return 1;
        }
    }

    if (verbose) {
        TML_LOG_INFO("mcp", "Starting TML MCP server (standalone)...");
        TML_LOG_INFO("mcp", "Transport: stdio");
        TML_LOG_INFO("mcp", "Protocol version: " << tml::mcp::MCP_PROTOCOL_VERSION);
    }

    // Create and configure server
    tml::mcp::McpServer server("tml-compiler", "0.1.0");

    // Register compiler tools
    tml::mcp::register_compiler_tools(server);

    if (verbose) {
        TML_LOG_INFO("mcp", "Server ready, waiting for requests...");
    }

    // Run server (blocks until shutdown)
    server.run();

    if (verbose) {
        TML_LOG_INFO("mcp", "Server shutdown complete.");
    }

    return 0;
}
