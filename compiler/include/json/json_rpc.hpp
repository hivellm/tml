//! # JSON-RPC 2.0 Types
//!
//! This module provides types for JSON-RPC 2.0 protocol support, primarily
//! for MCP (Model Context Protocol) integration.
//!
//! ## JSON-RPC 2.0 Overview
//!
//! JSON-RPC is a stateless, light-weight remote procedure call protocol.
//! A request contains:
//!
//! - `jsonrpc`: Version string (always "2.0")
//! - `method`: Name of the method to invoke
//! - `params`: Optional parameters (object or array)
//! - `id`: Request identifier (string, number, or null for notifications)
//!
//! A response contains:
//!
//! - `jsonrpc`: Version string (always "2.0")
//! - `result`: Success result (mutually exclusive with `error`)
//! - `error`: Error object (mutually exclusive with `result`)
//! - `id`: Must match the request id
//!
//! ## Error Codes
//!
//! | Code | Name | Description |
//! |------|------|-------------|
//! | -32700 | Parse error | Invalid JSON |
//! | -32600 | Invalid Request | Not a valid Request object |
//! | -32601 | Method not found | Method does not exist |
//! | -32602 | Invalid params | Invalid method parameters |
//! | -32603 | Internal error | Internal JSON-RPC error |
//! | -32000 to -32099 | Server error | Reserved for server errors |
//!
//! ## Example
//!
//! ```cpp
//! #include "json/json_rpc.hpp"
//! using namespace tml::json;
//!
//! // Parse a request
//! auto json = parse_json(R"({"jsonrpc":"2.0","method":"add","params":[1,2],"id":1})");
//! auto req = JsonRpcRequest::from_json(unwrap(json));
//! if (is_ok(req)) {
//!     auto& request = unwrap(req);
//!     // request.method == "add"
//!     // request.params->as_array()[0].as_i64() == 1
//! }
//!
//! // Create a response
//! auto response = JsonRpcResponse::success(JsonValue(3), JsonValue(1));
//! std::string output = response.to_json().to_string();
//! // {"id":1,"jsonrpc":"2.0","result":3}
//! ```

#pragma once

#include "common.hpp"
#include "json/json_value.hpp"

#include <optional>
#include <string>

namespace tml::json {

/// Standard JSON-RPC 2.0 error codes.
///
/// These are the predefined error codes from the JSON-RPC specification.
/// Server implementations may define additional codes in the range -32000 to -32099.
enum class JsonRpcErrorCode : int {
    ParseError = -32700,     ///< Invalid JSON was received
    InvalidRequest = -32600, ///< The JSON is not a valid Request object
    MethodNotFound = -32601, ///< The method does not exist
    InvalidParams = -32602,  ///< Invalid method parameter(s)
    InternalError = -32603,  ///< Internal JSON-RPC error
    ServerError = -32000     ///< Generic server error (start of range)
};

/// JSON-RPC 2.0 error object.
///
/// Represents an error that occurred during RPC processing.
/// Contains a numeric code, message, and optional additional data.
///
/// # Example
///
/// ```cpp
/// auto error = JsonRpcError::from_code(JsonRpcErrorCode::MethodNotFound);
/// // error.code == -32601
/// // error.message == "Method not found"
/// ```
struct JsonRpcError {
    /// A number indicating the error type.
    int code;

    /// A short description of the error.
    std::string message;

    /// Additional information about the error (optional).
    std::optional<JsonValue> data;

    /// Creates an error from a standard error code.
    ///
    /// # Arguments
    ///
    /// * `code` - The standard JSON-RPC error code
    ///
    /// # Returns
    ///
    /// A `JsonRpcError` with the appropriate code and message.
    static auto from_code(JsonRpcErrorCode code) -> JsonRpcError;

    /// Creates a custom error.
    ///
    /// # Arguments
    ///
    /// * `code` - The error code
    /// * `message` - The error message
    /// * `data` - Optional additional data
    ///
    /// # Returns
    ///
    /// A `JsonRpcError` with the specified values.
    static auto make(int code, std::string message,
                     std::optional<JsonValue> data = std::nullopt) -> JsonRpcError;

    /// Converts this error to a JSON object.
    ///
    /// # Returns
    ///
    /// A `JsonValue` object containing the error fields.
    [[nodiscard]] auto to_json() const -> JsonValue;
};

/// JSON-RPC 2.0 request object.
///
/// Represents a method invocation request from a client.
/// If `id` is not present, the request is a notification (no response expected).
///
/// # Example
///
/// ```cpp
/// // Parse from JSON
/// auto json = parse_json(R"({"jsonrpc":"2.0","method":"sum","params":[1,2,3],"id":"req-1"})");
/// auto req = JsonRpcRequest::from_json(unwrap(json));
///
/// // Check if notification
/// if (unwrap(req).is_notification()) {
///     // No response needed
/// }
/// ```
struct JsonRpcRequest {
    /// Protocol version (always "2.0").
    std::string jsonrpc = "2.0";

    /// Name of the method to be invoked.
    std::string method;

    /// Parameters for the method (optional).
    std::optional<JsonValue> params;

    /// Request identifier (optional; absent for notifications).
    std::optional<JsonValue> id;

    /// Parses a `JsonRpcRequest` from a `JsonValue`.
    ///
    /// # Arguments
    ///
    /// * `json` - The JSON value to parse
    ///
    /// # Returns
    ///
    /// `Ok(JsonRpcRequest)` on success, `Err(JsonRpcError)` on failure.
    [[nodiscard]] static auto from_json(const JsonValue& json)
        -> Result<JsonRpcRequest, JsonRpcError>;

    /// Converts this request to a JSON object.
    ///
    /// # Returns
    ///
    /// A `JsonValue` object containing the request fields.
    [[nodiscard]] auto to_json() const -> JsonValue;

    /// Returns `true` if this is a notification (no id).
    ///
    /// Notifications are requests that do not expect a response.
    [[nodiscard]] auto is_notification() const -> bool { return !id.has_value(); }
};

/// JSON-RPC 2.0 response object.
///
/// Represents the result of a method invocation.
/// Contains either a `result` (success) or `error` (failure), never both.
///
/// # Example
///
/// ```cpp
/// // Success response
/// auto success = JsonRpcResponse::success(JsonValue(42), JsonValue(1));
///
/// // Error response
/// auto error = JsonRpcResponse::failure(
///     JsonRpcError::from_code(JsonRpcErrorCode::InvalidParams),
///     JsonValue(1)
/// );
/// ```
struct JsonRpcResponse {
    /// Protocol version (always "2.0").
    std::string jsonrpc = "2.0";

    /// The result of the method invocation (for success).
    std::optional<JsonValue> result;

    /// The error object (for failure).
    std::optional<JsonRpcError> error;

    /// The request identifier that this response corresponds to.
    JsonValue id;

    /// Creates a success response.
    ///
    /// # Arguments
    ///
    /// * `result` - The method result
    /// * `id` - The request id to respond to
    ///
    /// # Returns
    ///
    /// A `JsonRpcResponse` with the result set.
    static auto success(JsonValue result, JsonValue id) -> JsonRpcResponse;

    /// Creates an error response.
    ///
    /// # Arguments
    ///
    /// * `error` - The error object
    /// * `id` - The request id to respond to
    ///
    /// # Returns
    ///
    /// A `JsonRpcResponse` with the error set.
    static auto failure(JsonRpcError error, JsonValue id) -> JsonRpcResponse;

    /// Parses a `JsonRpcResponse` from a `JsonValue`.
    ///
    /// # Arguments
    ///
    /// * `json` - The JSON value to parse
    ///
    /// # Returns
    ///
    /// `Ok(JsonRpcResponse)` on success, `Err(JsonRpcError)` on failure.
    [[nodiscard]] static auto from_json(const JsonValue& json)
        -> Result<JsonRpcResponse, JsonRpcError>;

    /// Converts this response to a JSON object.
    ///
    /// # Returns
    ///
    /// A `JsonValue` object containing the response fields.
    [[nodiscard]] auto to_json() const -> JsonValue;

    /// Returns `true` if this is an error response.
    [[nodiscard]] auto is_error() const -> bool { return error.has_value(); }
};

} // namespace tml::json
