TML_MODULE("mcp")

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

#include "log/log.hpp"

#include "json/json_parser.hpp"
#include <cstdlib>
#include <future>

namespace tml::mcp {

// ============================================================================
// Call Logger (NDJSON)
// ============================================================================

void McpServer::init_call_logger() {
    if (call_log_fp_ != nullptr) {
        return;
    }

    // Session ID from epoch ms
    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    session_id_ = std::to_string(epoch_ms);

    // Log path
    std::string log_path = "mcp-call-log.jsonl";
    const char* env_dir = std::getenv("TML_MCP_LOG_DIR");
    if (env_dir != nullptr && env_dir[0] != '\0') {
        log_path = std::string(env_dir) + "/mcp-call-log.jsonl";
    }

    call_log_fp_ = std::fopen(log_path.c_str(), "a");
    if (call_log_fp_ == nullptr) {
        return;
    }

    // Condition tracking — default is now "debug-layers" (Condition B).
    // Set TML_DEBUG_LAYERS=0 to revert to baseline for comparison.
    const char* dl_env = std::getenv("TML_DEBUG_LAYERS");
    const char* condition =
        (dl_env != nullptr && std::string(dl_env) == "0") ? "baseline" : "debug-layers";

    std::fprintf(call_log_fp_,
                 "{\"event\":\"session_start\",\"session\":\"%s\","
                 "\"server\":\"%s\",\"version\":\"%s\",\"condition\":\"%s\"}\n",
                 session_id_.c_str(), server_info_.name.c_str(), server_info_.version.c_str(),
                 condition);
    std::fflush(call_log_fp_);
}

void McpServer::log_tool_call(const std::string& tool_name, const std::string& params_json,
                              bool is_error, int64_t duration_ms) {
    std::lock_guard<std::mutex> lock(call_log_mutex_);
    if (call_log_fp_ == nullptr) {
        init_call_logger();
    }
    if (call_log_fp_ == nullptr) {
        return;
    }

    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    std::fprintf(call_log_fp_,
                 "{\"event\":\"tool_call\",\"session\":\"%s\",\"seq\":%llu,"
                 "\"ts\":%lld,\"tool\":\"%s\",\"params\":%s,"
                 "\"duration_ms\":%lld,\"is_error\":%s}\n",
                 session_id_.c_str(), static_cast<unsigned long long>(call_sequence_.fetch_add(1)),
                 static_cast<long long>(epoch_ms), tool_name.c_str(), params_json.c_str(),
                 static_cast<long long>(duration_ms), is_error ? "true" : "false");
    std::fflush(call_log_fp_);
}

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

    // Drain all active tool worker threads before shutting down.
    // Workers dispatch their responses directly; we just wait for them to finish.
    {
        std::unique_lock<std::mutex> lock(workers_mutex_);
        workers_cv_.wait(lock, [this] { return active_workers_.load() == 0; });
    }

    // Write session end marker
    if (call_log_fp_ != nullptr) {
        std::fprintf(call_log_fp_,
                     "{\"event\":\"session_end\",\"session\":\"%s\",\"total_calls\":%llu}\n",
                     session_id_.c_str(), static_cast<unsigned long long>(call_sequence_));
        std::fclose(call_log_fp_);
        call_log_fp_ = nullptr;
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

    // Clone id for each handler since JsonValue is move-only
    auto id = request.id.value().clone();

    if (request.method == "initialize") {
        auto params = request.params.has_value() ? request.params->clone() : json::JsonValue();
        send_response(handle_initialize(std::move(params), std::move(id)));
    } else if (request.method == "shutdown") {
        send_response(handle_shutdown(std::move(id)));
    } else if (request.method == "tools/list") {
        send_response(handle_tools_list(std::move(id)));
    } else if (request.method == "tools/call") {
        // Dispatch to worker thread — main stdin loop stays responsive.
        // If the tool hangs, it times out inside handle_tools_call and the
        // worker thread eventually returns; stdin reading is never blocked.
        auto params = request.params.has_value() ? request.params->clone() : json::JsonValue();
        active_workers_.fetch_add(1);
        std::thread([this, params = std::move(params), id = std::move(id)]() mutable {
            send_response(handle_tools_call(std::move(params), std::move(id)));
            {
                std::lock_guard<std::mutex> lock(workers_mutex_);
                active_workers_.fetch_sub(1);
            }
            workers_cv_.notify_all();
        }).detach();
    } else {
        send_response(json::JsonRpcResponse::failure(
            json::JsonRpcError::from_code(json::JsonRpcErrorCode::MethodNotFound), std::move(id)));
    }
}

// ============================================================================
// Response Sending
// ============================================================================

void McpServer::send_response(const json::JsonRpcResponse& response) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    std::cout << response.to_json().to_string() << "\n" << std::flush;
}

void McpServer::send_error(json::JsonValue id, json::JsonRpcErrorCode code,
                           const std::string& message) {
    auto response = json::JsonRpcResponse::failure(
        json::JsonRpcError::make(static_cast<int>(code), message), std::move(id));
    send_response(response);
}

void McpServer::log(const std::string& message) {
    TML_LOG_DEBUG("mcp", message);
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

    // Pre-serialize params for logging (safe — before handler call)
    std::string params_json = "{}";
    if (arguments != nullptr) {
        try {
            params_json = arguments->to_string();
        } catch (...) {
            params_json = "{}";
        }
    }

    // Run tool handler in an inner thread with a hard timeout.
    // This prevents any tool from hanging the worker thread indefinitely.
    // The worker thread was already dispatched by process_request, so the
    // main stdin loop is never blocked regardless of what happens here.
    constexpr int TOOL_TIMEOUT_S = 300;

    auto promise = std::make_shared<std::promise<ToolResult>>();
    auto future = promise->get_future();
    auto handler_fn = it->second; // copy handler for thread capture

    std::thread([promise, handler_fn, args = std::move(args)]() mutable {
        try {
            promise->set_value(handler_fn(args));
        } catch (const std::exception& e) {
            promise->set_value(ToolResult::error(e.what()));
        } catch (...) {
            promise->set_value(ToolResult::error("Unknown exception in tool handler"));
        }
    }).detach();

    auto start = std::chrono::steady_clock::now();
    ToolResult result;

    if (future.wait_for(std::chrono::seconds(TOOL_TIMEOUT_S)) == std::future_status::timeout) {
        result = ToolResult::error("Tool '" + tool_name + "' timed out after " +
                                   std::to_string(TOOL_TIMEOUT_S) + "s");
        log("Tool timed out: " + tool_name);
    } else {
        try {
            result = future.get();
        } catch (const std::exception& e) {
            result = ToolResult::error(e.what());
        } catch (...) {
            result = ToolResult::error("Unknown exception in tool '" + tool_name + "'");
        }
    }

    auto elapsed_ms = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - start)
                                               .count());
    log_tool_call(tool_name, params_json, result.is_error, elapsed_ms);
    return json::JsonRpcResponse::success(result.to_json(), std::move(id));
}

} // namespace tml::mcp
