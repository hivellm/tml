//! # MCP Server
//!
//! Model Context Protocol server implementation for the TML compiler.
//!
//! ## Overview
//!
//! This server enables AI assistants (like Claude) to interact with the TML
//! compiler programmatically via JSON-RPC 2.0 over stdio.
//!
//! ## Transport
//!
//! The server uses **stdio** transport:
//! - Reads JSON-RPC requests from stdin (one per line)
//! - Writes JSON-RPC responses to stdout (one per line)
//! - Writes logs to stderr
//!
//! ## Protocol Flow
//!
//! 1. Client sends `initialize` request
//! 2. Server responds with capabilities
//! 3. Client sends `initialized` notification
//! 4. Client calls tools via `tools/call`
//! 5. Client sends `shutdown` request to terminate
//!
//! ## Example
//!
//! ```cpp
//! #include "mcp/mcp_server.hpp"
//!
//! mcp::McpServer server;
//! server.register_tool(compile_tool, compile_handler);
//! server.run(); // Blocks, processing stdio
//! ```
//!
//! ## Thread Safety
//!
//! The server is single-threaded and processes requests sequentially.

#pragma once

#include "mcp/mcp_types.hpp"

#include "json/json_rpc.hpp"
#include "json/json_value.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::mcp {

/// Tool handler function type.
///
/// Receives the tool parameters and returns a result.
using ToolHandler = std::function<ToolResult(const json::JsonValue& params)>;

/// MCP Server implementation.
///
/// Implements the Model Context Protocol over stdio transport.
/// Register tools with `register_tool()` and call `run()` to start.
class McpServer {
public:
    /// Creates a new MCP server.
    ///
    /// # Arguments
    ///
    /// * `name` - Server name (sent during initialization)
    /// * `version` - Server version
    McpServer(std::string name = "tml-compiler", std::string version = "0.1.0");

    /// Registers a tool with its handler.
    ///
    /// # Arguments
    ///
    /// * `tool` - Tool definition
    /// * `handler` - Function to handle tool invocations
    void register_tool(Tool tool, ToolHandler handler);

    /// Runs the server, processing stdio.
    ///
    /// This function blocks until shutdown is requested or stdin closes.
    /// Reads JSON-RPC requests from stdin and writes responses to stdout.
    void run();

    /// Stops the server.
    ///
    /// Call this from a tool handler to request shutdown.
    void stop();

    /// Returns true if the server is running.
    [[nodiscard]] bool is_running() const {
        return running_;
    }

private:
    // Server identity
    ServerInfo server_info_;
    ServerCapabilities capabilities_;

    // Client info (set after initialization)
    std::optional<ClientInfo> client_info_;
    bool initialized_ = false;
    bool running_ = false;

    // Registered tools
    std::vector<Tool> tools_;
    std::unordered_map<std::string, ToolHandler> tool_handlers_;

    // Request processing
    void process_request(const json::JsonRpcRequest& request);
    void send_response(const json::JsonRpcResponse& response);
    void send_error(json::JsonValue id, json::JsonRpcErrorCode code, const std::string& message);
    void log(const std::string& message);

    // Protocol handlers
    auto handle_initialize(json::JsonValue params, json::JsonValue id) -> json::JsonRpcResponse;
    void handle_initialized();
    auto handle_shutdown(json::JsonValue id) -> json::JsonRpcResponse;
    auto handle_tools_list(json::JsonValue id) -> json::JsonRpcResponse;
    auto handle_tools_call(json::JsonValue params, json::JsonValue id) -> json::JsonRpcResponse;
};

} // namespace tml::mcp
