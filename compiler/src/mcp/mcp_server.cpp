//! # MCP Server Implementation
//!
//! Implements the Model Context Protocol server over stdio transport.
//!
//! ## Transport Protocol
//!
//! - Reads newline-delimited JSON-RPC messages from stdin
//! - Writes newline-delimited JSON-RPC messages to stdout
//! - Logs are written to stderr
//!
//! ## Message Flow
//!
//! 1. Read line from stdin
//! 2. Parse as JSON
//! 3. Parse as JSON-RPC request
//! 4. Route to appropriate handler
//! 5. Write response to stdout

#include "mcp/mcp_server.hpp"

#include "json/json_parser.hpp"

#include <iostream>

namespace tml::mcp {

// ============================================================================
// Constructor
// ============================================================================

McpServer::McpServer(std::string name, std::string version)
    : server_info_{std::move(name), std::move(version)} {
    capabilities_.tools = true;
    capabilities_.resources = false;
    capabilities_.prompts = false;
}

// ============================================================================
// Tool Registration
// ============================================================================

void McpServer::register_tool(Tool tool, ToolHandler handler) {
    tool_handlers_[tool.name] = std::move(handler);
    tools_.push_back(std::move(tool));
}

// ============================================================================
// Main Loop
// ============================================================================

void McpServer::run() {
    running_ = true;
    log("MCP server starting (" + server_info_.name + " v" + server_info_.version + ")");

    std::string line;
    while (running_ && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        // Parse JSON
        auto json_result = json::parse_json(line);
        if (is_err(json_result)) {
            log("Parse error: " + unwrap_err(json_result).message);
            send_error(json::JsonValue(), json::JsonRpcErrorCode::ParseError,
                       unwrap_err(json_result).message);
            continue;
        }

        // Parse JSON-RPC request
        auto request_result = json::JsonRpcRequest::from_json(unwrap(json_result));
        if (is_err(request_result)) {
            log("Invalid request: " + unwrap_err(request_result).message);
            send_error(json::JsonValue(), json::JsonRpcErrorCode::InvalidRequest,
                       unwrap_err(request_result).message);
            continue;
        }

        process_request(unwrap(request_result));
    }

    log("MCP server stopped");
}

void McpServer::stop() {
    running_ = false;
}

// ============================================================================
// Request Processing
// ============================================================================

void McpServer::process_request(const json::JsonRpcRequest& request) {
    log("Request: " + request.method);

    // Handle notifications (no id)
    if (request.is_notification()) {
        if (request.method == "notifications/initialized") {
            handle_initialized();
        } else if (request.method == "notifications/cancelled") {
            // Ignore cancellation for now
        } else {
            log("Unknown notification: " + request.method);
        }
        return;
    }

    // Handle requests (have id)
    json::JsonRpcResponse response;

    // Clone id for each handler since JsonValue is move-only
    auto id = request.id.value().clone();

    if (request.method == "initialize") {
        auto params = request.params.has_value() ? request.params->clone() : json::JsonValue();
        response = handle_initialize(std::move(params), std::move(id));
    } else if (request.method == "shutdown") {
        response = handle_shutdown(std::move(id));
    } else if (request.method == "tools/list") {
        response = handle_tools_list(std::move(id));
    } else if (request.method == "tools/call") {
        auto params = request.params.has_value() ? request.params->clone() : json::JsonValue();
        response = handle_tools_call(std::move(params), std::move(id));
    } else {
        response = json::JsonRpcResponse::failure(
            json::JsonRpcError::from_code(json::JsonRpcErrorCode::MethodNotFound),
            std::move(id));
    }

    send_response(response);
}

// ============================================================================
// Response Sending
// ============================================================================

void McpServer::send_response(const json::JsonRpcResponse& response) {
    std::cout << response.to_json().to_string() << "\n" << std::flush;
}

void McpServer::send_error(json::JsonValue id, json::JsonRpcErrorCode code,
                           const std::string& message) {
    auto response = json::JsonRpcResponse::failure(
        json::JsonRpcError::make(static_cast<int>(code), message), std::move(id));
    send_response(response);
}

void McpServer::log(const std::string& message) {
    std::cerr << "[MCP] " << message << std::endl;
}

// ============================================================================
// Protocol Handlers
// ============================================================================

auto McpServer::handle_initialize(json::JsonValue params, json::JsonValue id)
    -> json::JsonRpcResponse {
    // Extract client info
    if (params.is_object()) {
        auto* client_info = params.get("clientInfo");
        if (client_info != nullptr) {
            client_info_ = ClientInfo::from_json(*client_info);
            if (client_info_.has_value()) {
                log("Client: " + client_info_->name + " v" + client_info_->version);
            }
        }
    }

    // Build response
    json::JsonObject result;
    result["protocolVersion"] = json::JsonValue(MCP_PROTOCOL_VERSION);
    result["capabilities"] = capabilities_.to_json();
    result["serverInfo"] = server_info_.to_json();

    initialized_ = true;
    return json::JsonRpcResponse::success(json::JsonValue(std::move(result)), std::move(id));
}

void McpServer::handle_initialized() {
    log("Initialization complete");
}

auto McpServer::handle_shutdown(json::JsonValue id) -> json::JsonRpcResponse {
    log("Shutdown requested");
    stop();
    return json::JsonRpcResponse::success(json::JsonValue(), std::move(id));
}

auto McpServer::handle_tools_list(json::JsonValue id) -> json::JsonRpcResponse {
    json::JsonArray tools_arr;
    for (const auto& tool : tools_) {
        tools_arr.push_back(tool.to_json());
    }

    json::JsonObject result;
    result["tools"] = json::JsonValue(std::move(tools_arr));
    return json::JsonRpcResponse::success(json::JsonValue(std::move(result)), std::move(id));
}

auto McpServer::handle_tools_call(json::JsonValue params, json::JsonValue id)
    -> json::JsonRpcResponse {
    // Extract tool name
    if (!params.is_object()) {
        return json::JsonRpcResponse::failure(
            json::JsonRpcError::make(static_cast<int>(json::JsonRpcErrorCode::InvalidParams),
                                     "params must be an object"),
            std::move(id));
    }

    auto* name_val = params.get("name");
    if (name_val == nullptr || !name_val->is_string()) {
        return json::JsonRpcResponse::failure(
            json::JsonRpcError::make(static_cast<int>(json::JsonRpcErrorCode::InvalidParams),
                                     "missing or invalid tool name"),
            std::move(id));
    }

    std::string tool_name = name_val->as_string();
    log("Calling tool: " + tool_name);

    // Find handler
    auto it = tool_handlers_.find(tool_name);
    if (it == tool_handlers_.end()) {
        return json::JsonRpcResponse::failure(
            json::JsonRpcError::make(static_cast<int>(json::JsonRpcErrorCode::MethodNotFound),
                                     "tool not found: " + tool_name),
            std::move(id));
    }

    // Get arguments
    auto* arguments = params.get("arguments");
    json::JsonValue args = (arguments != nullptr) ? arguments->clone() : json::JsonValue();

    // Call handler
    try {
        ToolResult result = it->second(args);
        return json::JsonRpcResponse::success(result.to_json(), std::move(id));
    } catch (const std::exception& e) {
        log("Tool error: " + std::string(e.what()));
        return json::JsonRpcResponse::success(ToolResult::error(e.what()).to_json(), std::move(id));
    }
}

} // namespace tml::mcp
