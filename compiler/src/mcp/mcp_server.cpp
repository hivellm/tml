TML_MODULE("mcp")

//! # MCP Server Implementation
//!
//! Implements the Model Context Protocol server over stdio and Streamable HTTP transports.
//!
//! ## Stdio Transport (default)
//!
//! - Reads newline-delimited JSON-RPC messages from stdin
//! - Writes newline-delimited JSON-RPC messages to stdout
//! - Logs are written to stderr
//!
//! ## Streamable HTTP Transport (--http)
//!
//! - Listens on localhost:PORT for POST /mcp requests
//! - Extracts JSON-RPC body, processes it, returns HTTP 200 with JSON-RPC response
//! - Runs as a persistent daemon until killed
//!
//! ## Message Flow
//!
//! 1. Read line from stdin / Read HTTP POST body
//! 2. Parse as JSON
//! 3. Parse as JSON-RPC request
//! 4. Route to appropriate handler
//! 5. Write response to stdout / Send HTTP response

#include "mcp/mcp_server.hpp"

#include "log/log.hpp"

#include "json/json_parser.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>

// Platform-specific socket headers
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
static constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
static inline int close_socket(socket_t s) {
    return closesocket(s);
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t INVALID_SOCK = -1;
static inline int close_socket(socket_t s) {
    return close(s);
}
#endif

namespace fs = std::filesystem;

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
    const char* dl_env = std::getenv("TML_DEBUG_LAYERS");
    const char* condition =
        (dl_env != nullptr && std::string(dl_env) == "0") ? "baseline" : "debug-layers";

    // 2.1: Detect active Rulebook task
    std::string task_id;
    {
        std::error_code ec;
        fs::path tasks_dir = fs::current_path() / ".rulebook" / "tasks";
        if (fs::exists(tasks_dir, ec)) {
            for (const auto& entry : fs::directory_iterator(tasks_dir, ec)) {
                if (!entry.is_directory())
                    continue;
                fs::path meta = entry.path() / ".metadata.json";
                if (!fs::exists(meta))
                    continue;
                // Read metadata to check status
                std::ifstream mf(meta);
                std::string content((std::istreambuf_iterator<char>(mf)),
                                    std::istreambuf_iterator<char>());
                if (content.find("\"in-progress\"") != std::string::npos) {
                    task_id = entry.path().filename().string();
                    break;
                }
            }
        }
    }

    // 2.2: Model from env var
    std::string model;
    const char* model_env = std::getenv("TML_MODEL");
    if (model_env != nullptr && model_env[0] != '\0') {
        model = model_env;
    }

    // 2.4: CLAUDE.md hash (CRC32 of first 4KB for speed)
    std::string claude_md_hash;
    {
        std::ifstream cf("CLAUDE.md", std::ios::binary);
        if (cf.is_open()) {
            char buf[4096];
            cf.read(buf, sizeof(buf));
            auto n = cf.gcount();
            uint32_t crc = 0;
            for (std::streamsize i = 0; i < n; i++) {
                crc = crc ^ static_cast<uint32_t>(static_cast<uint8_t>(buf[i]));
                for (int j = 0; j < 8; j++) {
                    crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
                }
            }
            char hex[9];
            std::snprintf(hex, sizeof(hex), "%08x", crc);
            claude_md_hash = hex;
        }
    }

    std::fprintf(call_log_fp_,
                 "{\"event\":\"session_start\",\"session\":\"%s\","
                 "\"server\":\"%s\",\"version\":\"%s\",\"condition\":\"%s\""
                 ",\"task\":\"%s\",\"model\":\"%s\",\"claude_md_hash\":\"%s\"}\n",
                 session_id_.c_str(), server_info_.name.c_str(), server_info_.version.c_str(),
                 condition, task_id.c_str(), model.c_str(), claude_md_hash.c_str());
    std::fflush(call_log_fp_);
}

void McpServer::log_tool_call(const std::string& tool_name, const std::string& params_json,
                              bool is_error, int64_t duration_ms, const std::string& result_text) {
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

    // --- 1.1: Extract test results from structured output ---
    std::string test_results;
    if (tool_name == "test" && !result_text.empty()) {
        auto total_pos = result_text.find("\"total\":");
        if (total_pos != std::string::npos) {
            auto brace_start = result_text.rfind('{', total_pos);
            auto brace_end = result_text.find('}', total_pos);
            if (brace_start != std::string::npos && brace_end != std::string::npos) {
                test_results = result_text.substr(brace_start, brace_end - brace_start + 1);
            }
        }
    }

    // --- 1.2: Extract normalized test target (suite or path) ---
    std::string test_target;
    if (tool_name == "test") {
        auto suite_pos = params_json.find("\"suite\":");
        if (suite_pos != std::string::npos) {
            auto quote_start = params_json.find('"', suite_pos + 8);
            auto quote_end = params_json.find('"', quote_start + 1);
            if (quote_start != std::string::npos && quote_end != std::string::npos) {
                test_target = params_json.substr(quote_start + 1, quote_end - quote_start - 1);
            }
        } else {
            auto path_pos = params_json.find("\"path\":");
            if (path_pos != std::string::npos) {
                auto quote_start = params_json.find('"', path_pos + 7);
                auto quote_end = params_json.find('"', quote_start + 1);
                if (quote_start != std::string::npos && quote_end != std::string::npos) {
                    test_target = params_json.substr(quote_start + 1, quote_end - quote_start - 1);
                }
            }
        }
    }

    // --- 1.3: Check if debug_layers was explicitly requested ---
    // -1 = not applicable (non-test), 0 = false, 1 = true
    int debug_layers_state = -1;
    if (tool_name == "test") {
        debug_layers_state =
            (params_json.find("\"debug_layers\":true") != std::string::npos) ? 1 : 0;
    }

    // --- 1.4: Estimate output token count (rough: chars/4) ---
    int64_t output_tokens = static_cast<int64_t>(result_text.size()) / 4;

    // --- 1.5: Classify error type ---
    std::string error_type = "none";
    if (is_error) {
        if (result_text.find("CRASH") != std::string::npos ||
            result_text.find("HEAP_CORRUPTION") != std::string::npos) {
            error_type = "crash";
        } else if (result_text.find("TIMEOUT") != std::string::npos) {
            error_type = "timeout";
        } else if (result_text.find("compile_errors") != std::string::npos ||
                   result_text.find("Compile error") != std::string::npos ||
                   result_text.find("Type check failed") != std::string::npos) {
            error_type = "compile";
        } else if (result_text.find("assertion") != std::string::npos ||
                   result_text.find("FAIL") != std::string::npos ||
                   result_text.find("failed") != std::string::npos) {
            error_type = "assertion";
        } else {
            error_type = "unknown";
        }
    } else if (tool_name == "test" && !result_text.empty()) {
        // Check for test failures even when is_error is false
        auto pos = result_text.find("\"failed\":");
        if (pos != std::string::npos) {
            auto num_start = pos + 9;
            while (num_start < result_text.size() && result_text[num_start] == ' ') {
                num_start++;
            }
            if (num_start < result_text.size() && result_text[num_start] != '0') {
                error_type = "assertion";
            }
        }
    }

    // --- 1.6: Track preceding tool ---
    std::string preceded_by = last_tool_name_;
    last_tool_name_ = tool_name;

    // --- Build debug_layers JSON value ---
    const char* debug_layers_json =
        (debug_layers_state < 0) ? "null" : (debug_layers_state ? "true" : "false");

    // --- Build optional test_results suffix (avoids allocation when empty) ---
    std::string test_results_suffix;
    if (!test_results.empty()) {
        test_results_suffix = ",\"test_results\":" + test_results;
    }

    std::fprintf(call_log_fp_,
                 "{\"event\":\"tool_call\",\"session\":\"%s\",\"seq\":%llu,"
                 "\"ts\":%lld,\"tool\":\"%s\",\"params\":%s,"
                 "\"duration_ms\":%lld,\"is_error\":%s,"
                 "\"error_type\":\"%s\",\"preceded_by\":\"%s\","
                 "\"test_target\":\"%s\",\"debug_layers\":%s,"
                 "\"output_tokens\":%lld%s}\n",
                 session_id_.c_str(), static_cast<unsigned long long>(call_sequence_.fetch_add(1)),
                 static_cast<long long>(epoch_ms), tool_name.c_str(), params_json.c_str(),
                 static_cast<long long>(duration_ms), is_error ? "true" : "false",
                 error_type.c_str(), preceded_by.c_str(), test_target.c_str(), debug_layers_json,
                 static_cast<long long>(output_tokens), test_results_suffix.c_str());
    std::fflush(call_log_fp_);

    // Track stats for session summary (2.3)
    tool_counts_[tool_name]++;
    if (is_error || error_type != "none") {
        error_count_++;
    }
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

    write_session_end_log();
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

    // Extract result text for logging
    std::string result_text;
    for (const auto& c : result.content) {
        if (c.type == "text" && !c.text.empty()) {
            result_text = c.text;
            break;
        }
    }

    log_tool_call(tool_name, params_json, result.is_error, elapsed_ms, result_text);
    return json::JsonRpcResponse::success(result.to_json(), std::move(id));
}

// ============================================================================
// Session End Logging (shared by stdio and HTTP transports)
// ============================================================================

void McpServer::write_session_end_log() {
    if (call_log_fp_ == nullptr) {
        return;
    }

    auto total = static_cast<unsigned long long>(call_sequence_.load());

    // Find dominant tool
    std::string dominant_tool;
    int max_count = 0;
    int tools_used = 0;
    for (const auto& [tool, count] : tool_counts_) {
        tools_used++;
        if (count > max_count) {
            max_count = count;
            dominant_tool = tool;
        }
    }

    double error_rate = total > 0 ? static_cast<double>(error_count_) / total : 0.0;

    std::fprintf(call_log_fp_,
                 "{\"event\":\"session_end\",\"session\":\"%s\","
                 "\"total_calls\":%llu,\"tools_used\":%d,"
                 "\"error_count\":%d,\"error_rate\":%.3f,"
                 "\"dominant_tool\":\"%s\"}\n",
                 session_id_.c_str(), total, tools_used, error_count_, error_rate,
                 dominant_tool.c_str());
    std::fclose(call_log_fp_);
    call_log_fp_ = nullptr;
}

// ============================================================================
// Streamable HTTP Transport
// ============================================================================

void McpServer::run_http(int port) {
    running_ = true;
    log("MCP HTTP server starting on port " + std::to_string(port));

#ifdef _WIN32
    WSADATA wsa_data{};
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_result != 0) {
        log("WSAStartup failed with error: " + std::to_string(wsa_result));
        return;
    }
#endif

    socket_t server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCK) {
        log("Failed to create socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    // Allow port reuse so we can restart quickly
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt),
               sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost only — security
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        log("Failed to bind to port " + std::to_string(port));
        close_socket(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    if (listen(server_fd, 10) != 0) {
        log("Failed to listen on socket");
        close_socket(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    log("MCP HTTP server listening on http://localhost:" + std::to_string(port) + "/mcp");

    // Print to stderr so monitoring tools can detect readiness
    std::fprintf(stderr, "MCP HTTP server ready on http://localhost:%d/mcp\n", port);
    std::fflush(stderr);

    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        socket_t client_fd =
            accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (client_fd == INVALID_SOCK) {
            if (running_) {
                log("accept() failed, continuing...");
            }
            continue;
        }

        handle_http_connection(static_cast<uintptr_t>(client_fd));
    }

    // Drain active worker threads before shutting down
    {
        std::unique_lock<std::mutex> lock(workers_mutex_);
        workers_cv_.wait(lock, [this] { return active_workers_.load() == 0; });
    }

    close_socket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif

    write_session_end_log();
    log("MCP HTTP server stopped");
}

void McpServer::handle_http_connection(uintptr_t client_socket) {
    auto client_fd = static_cast<socket_t>(client_socket);

    // Read the full HTTP request (headers + body)
    std::string request;
    char buf[8192];

    // Read until we have the complete headers
    while (true) {
        int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            close_socket(client_fd);
            return;
        }
        buf[n] = '\0';
        request.append(buf, static_cast<size_t>(n));

        // Check if we have the full headers (delimited by \r\n\r\n)
        auto header_end = request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            // Parse Content-Length to know how much body to expect
            size_t content_length = 0;
            auto cl_pos = request.find("Content-Length: ");
            if (cl_pos == std::string::npos) {
                cl_pos = request.find("content-length: ");
            }
            if (cl_pos != std::string::npos) {
                auto cl_val_start = cl_pos + 16; // length of "Content-Length: "
                auto cl_val_end = request.find("\r\n", cl_val_start);
                if (cl_val_end != std::string::npos) {
                    content_length = static_cast<size_t>(
                        std::stoul(request.substr(cl_val_start, cl_val_end - cl_val_start)));
                }
            }

            size_t body_start = header_end + 4;
            size_t body_received = request.size() - body_start;

            // Read remaining body bytes if needed
            while (body_received < content_length) {
                size_t remaining = content_length - body_received;
                size_t to_read = (sizeof(buf) - 1 < remaining) ? sizeof(buf) - 1 : remaining;
                n = recv(client_fd, buf, static_cast<int>(to_read), 0);
                if (n <= 0) {
                    break;
                }
                buf[n] = '\0';
                request.append(buf, static_cast<size_t>(n));
                body_received += static_cast<size_t>(n);
            }
            break;
        }

        // Safety: don't let headers exceed 64KB
        if (request.size() > 65536) {
            send_http_response(client_socket, 413, "{\"error\":\"Request too large\"}");
            close_socket(client_fd);
            return;
        }
    }

    // Split headers and body
    auto header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        close_socket(client_fd);
        return;
    }
    std::string headers = request.substr(0, header_end);
    std::string body = request.substr(header_end + 4);

    // Parse the request line (first line of headers)
    auto first_line_end = headers.find("\r\n");
    std::string request_line =
        (first_line_end != std::string::npos) ? headers.substr(0, first_line_end) : headers;

    // Handle CORS preflight (OPTIONS)
    if (request_line.starts_with("OPTIONS ")) {
        std::string cors_response = "HTTP/1.1 204 No Content\r\n"
                                    "Access-Control-Allow-Origin: *\r\n"
                                    "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                                    "Access-Control-Allow-Headers: Content-Type\r\n"
                                    "Access-Control-Max-Age: 86400\r\n"
                                    "Content-Length: 0\r\n"
                                    "\r\n";
        send(client_fd, cors_response.c_str(), static_cast<int>(cors_response.size()), 0);
        close_socket(client_fd);
        return;
    }

    // Only accept POST /mcp or POST /
    bool is_valid_post =
        request_line.starts_with("POST /mcp") || request_line.starts_with("POST / ");
    if (!is_valid_post) {
        send_http_response(client_socket, 404,
                           "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32000,"
                           "\"message\":\"Not found. Use POST /mcp\"},\"id\":null}");
        close_socket(client_fd);
        return;
    }

    // Parse the JSON body
    if (body.empty()) {
        send_http_response(client_socket, 400,
                           "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,"
                           "\"message\":\"Empty request body\"},\"id\":null}");
        close_socket(client_fd);
        return;
    }

    auto json_result = json::parse_json(body);
    if (is_err(json_result)) {
        send_http_response(client_socket, 200,
                           "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,"
                           "\"message\":\"Parse error\"},\"id\":null}");
        close_socket(client_fd);
        return;
    }

    auto request_result = json::JsonRpcRequest::from_json(unwrap(json_result));
    if (is_err(request_result)) {
        send_http_response(client_socket, 200,
                           "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,"
                           "\"message\":\"Invalid request\"},\"id\":null}");
        close_socket(client_fd);
        return;
    }

    auto req = std::move(unwrap(request_result));

    // Handle notifications (no response needed per JSON-RPC spec)
    if (req.is_notification()) {
        // Process the notification (e.g., initialized)
        if (req.method == "notifications/initialized") {
            handle_initialized();
        }
        // Return 204 No Content for notifications
        std::string no_content = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, no_content.c_str(), static_cast<int>(no_content.size()), 0);
        close_socket(client_fd);
        return;
    }

    // Route the request to the appropriate handler
    auto id = req.id.value().clone();
    json::JsonRpcResponse response;

    if (req.method == "initialize") {
        auto params = req.params.has_value() ? req.params->clone() : json::JsonValue();
        response = handle_initialize(std::move(params), std::move(id));
    } else if (req.method == "shutdown") {
        response = handle_shutdown(std::move(id));
    } else if (req.method == "tools/list") {
        response = handle_tools_list(std::move(id));
    } else if (req.method == "tools/call") {
        auto params = req.params.has_value() ? req.params->clone() : json::JsonValue();
        response = handle_tools_call(std::move(params), std::move(id));
    } else {
        response = json::JsonRpcResponse::failure(
            json::JsonRpcError::make(-32601, "Method not found: " + req.method), std::move(id));
    }

    std::string response_body = response.to_json().to_string();
    send_http_response(client_socket, 200, response_body);
    close_socket(client_fd);
}

void McpServer::send_http_response(uintptr_t client_socket, int status_code,
                                   const std::string& body) {
    auto client_fd = static_cast<socket_t>(client_socket);

    const char* status_text = "OK";
    if (status_code == 204) {
        status_text = "No Content";
    } else if (status_code == 400) {
        status_text = "Bad Request";
    } else if (status_code == 404) {
        status_text = "Not Found";
    } else if (status_code == 413) {
        status_text = "Payload Too Large";
    }

    std::string response = "HTTP/1.1 " + std::to_string(status_code) + " " + status_text +
                           "\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: " +
                           std::to_string(body.size()) +
                           "\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Connection: close\r\n"
                           "\r\n" +
                           body;
    send(client_fd, response.c_str(), static_cast<int>(response.size()), 0);
}

} // namespace tml::mcp
