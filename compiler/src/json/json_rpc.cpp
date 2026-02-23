TML_MODULE("compiler")

//! # JSON-RPC 2.0 Implementation
//!
//! This module implements JSON-RPC 2.0 request/response handling for the TML compiler.
//! It provides serialization and deserialization of JSON-RPC messages.
//!
//! ## Features
//!
//! - **Request parsing**: Parse JSON-RPC requests from `JsonValue`
//! - **Response building**: Create success and error responses
//! - **Error handling**: Standard error codes with custom message support
//! - **Notification support**: Requests without `id` are treated as notifications
//!
//! ## Implementation Notes
//!
//! | Component | Description |
//! |-----------|-------------|
//! | `from_json` | Validates JSON structure and extracts fields |
//! | `to_json` | Converts structs to `JsonValue` for serialization |
//! | `from_code` | Maps standard error codes to messages |

#include "json/json_rpc.hpp"

namespace tml::json {

// ============================================================================
// JsonRpcError Implementation
// ============================================================================

/// Creates an error from a standard JSON-RPC error code.
///
/// Maps the error code to its standard message according to the JSON-RPC 2.0
/// specification.
///
/// # Arguments
///
/// * `code` - The standard error code
///
/// # Returns
///
/// A `JsonRpcError` with the appropriate code and message.
///
/// # Example
///
/// ```cpp
/// auto error = JsonRpcError::from_code(JsonRpcErrorCode::ParseError);
/// // error.code == -32700
/// // error.message == "Parse error"
/// ```
auto JsonRpcError::from_code(JsonRpcErrorCode code) -> JsonRpcError {
    std::string message;
    switch (code) {
    case JsonRpcErrorCode::ParseError:
        message = "Parse error";
        break;
    case JsonRpcErrorCode::InvalidRequest:
        message = "Invalid Request";
        break;
    case JsonRpcErrorCode::MethodNotFound:
        message = "Method not found";
        break;
    case JsonRpcErrorCode::InvalidParams:
        message = "Invalid params";
        break;
    case JsonRpcErrorCode::InternalError:
        message = "Internal error";
        break;
    case JsonRpcErrorCode::ServerError:
        message = "Server error";
        break;
    }
    return JsonRpcError{static_cast<int>(code), std::move(message), std::nullopt};
}

/// Creates a custom JSON-RPC error.
///
/// Use this for application-specific errors not covered by standard codes.
///
/// # Arguments
///
/// * `code` - The error code (use -32000 to -32099 for server errors)
/// * `message` - A descriptive error message
/// * `data` - Optional additional error data
///
/// # Returns
///
/// A `JsonRpcError` with the specified values.
///
/// # Example
///
/// ```cpp
/// auto error = JsonRpcError::make(-32001, "Resource not found",
///     JsonValue("user_id: 123"));
/// ```
auto JsonRpcError::make(int code, std::string message, std::optional<JsonValue> data)
    -> JsonRpcError {
    return JsonRpcError{code, std::move(message), std::move(data)};
}

/// Converts this error to a JSON object.
///
/// The resulting object has the structure:
/// ```json
/// {
///   "code": -32600,
///   "message": "Invalid Request",
///   "data": ...  // optional
/// }
/// ```
///
/// # Returns
///
/// A `JsonValue` object containing the error fields.
auto JsonRpcError::to_json() const -> JsonValue {
    JsonObject obj;
    obj["code"] = JsonValue(static_cast<int64_t>(code));
    obj["message"] = JsonValue(message);
    if (data.has_value()) {
        obj["data"] = data->clone();
    }
    return JsonValue(std::move(obj));
}

// ============================================================================
// JsonRpcRequest Implementation
// ============================================================================

/// Parses a JSON-RPC request from a `JsonValue`.
///
/// Validates that the input is a valid JSON-RPC 2.0 request object.
///
/// # Arguments
///
/// * `json` - The JSON value to parse
///
/// # Returns
///
/// `Ok(JsonRpcRequest)` if the input is a valid request.
/// `Err(JsonRpcError)` if validation fails.
///
/// # Validation Rules
///
/// - Must be an object
/// - Must have `jsonrpc` field equal to "2.0"
/// - Must have `method` field as a string
/// - `params` field is optional (object or array if present)
/// - `id` field is optional (absent means notification)
///
/// # Example
///
/// ```cpp
/// auto json = parse_json(R"({"jsonrpc":"2.0","method":"sum","params":[1,2],"id":1})");
/// auto result = JsonRpcRequest::from_json(unwrap(json));
/// if (is_ok(result)) {
///     auto& req = unwrap(result);
///     // req.method == "sum"
/// }
/// ```
auto JsonRpcRequest::from_json(const JsonValue& json) -> Result<JsonRpcRequest, JsonRpcError> {
    // Must be an object
    if (!json.is_object()) {
        return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                  "Request must be an object");
    }

    // Check jsonrpc version
    auto* version = json.get("jsonrpc");
    if (version == nullptr || !version->is_string() || version->as_string() != "2.0") {
        return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                  "Missing or invalid jsonrpc version");
    }

    // Check method
    auto* method = json.get("method");
    if (method == nullptr || !method->is_string()) {
        return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                  "Missing or invalid method");
    }

    JsonRpcRequest request;
    request.jsonrpc = "2.0";
    request.method = method->as_string();

    // Optional params
    auto* params = json.get("params");
    if (params != nullptr) {
        if (!params->is_array() && !params->is_object()) {
            return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidParams),
                                      "params must be an array or object");
        }
        request.params = params->clone();
    }

    // Optional id (absent means notification)
    auto* id = json.get("id");
    if (id != nullptr) {
        request.id = id->clone();
    }

    return request;
}

/// Converts this request to a JSON object.
///
/// The resulting object has the structure:
/// ```json
/// {
///   "jsonrpc": "2.0",
///   "method": "methodName",
///   "params": [...],  // optional
///   "id": 1           // optional (absent for notifications)
/// }
/// ```
///
/// # Returns
///
/// A `JsonValue` object containing the request fields.
auto JsonRpcRequest::to_json() const -> JsonValue {
    JsonObject obj;
    obj["jsonrpc"] = JsonValue(jsonrpc);
    obj["method"] = JsonValue(method);

    if (params.has_value()) {
        obj["params"] = params->clone();
    }

    if (id.has_value()) {
        obj["id"] = id->clone();
    }

    return JsonValue(std::move(obj));
}

// ============================================================================
// JsonRpcResponse Implementation
// ============================================================================

/// Creates a success response.
///
/// # Arguments
///
/// * `result` - The method result value
/// * `id` - The request id to respond to
///
/// # Returns
///
/// A `JsonRpcResponse` with the result set and no error.
///
/// # Example
///
/// ```cpp
/// auto response = JsonRpcResponse::success(JsonValue(42), JsonValue(1));
/// // Serializes to: {"id":1,"jsonrpc":"2.0","result":42}
/// ```
auto JsonRpcResponse::success(JsonValue result, JsonValue id) -> JsonRpcResponse {
    JsonRpcResponse response;
    response.result = std::move(result);
    response.id = std::move(id);
    return response;
}

/// Creates an error response.
///
/// # Arguments
///
/// * `error` - The error object describing what went wrong
/// * `id` - The request id to respond to
///
/// # Returns
///
/// A `JsonRpcResponse` with the error set and no result.
///
/// # Example
///
/// ```cpp
/// auto response = JsonRpcResponse::failure(
///     JsonRpcError::from_code(JsonRpcErrorCode::MethodNotFound),
///     JsonValue(1)
/// );
/// // Serializes to: {"error":{"code":-32601,"message":"Method not found"},"id":1,"jsonrpc":"2.0"}
/// ```
auto JsonRpcResponse::failure(JsonRpcError error, JsonValue id) -> JsonRpcResponse {
    JsonRpcResponse response;
    response.error = std::move(error);
    response.id = std::move(id);
    return response;
}

/// Parses a JSON-RPC response from a `JsonValue`.
///
/// Validates that the input is a valid JSON-RPC 2.0 response object.
///
/// # Arguments
///
/// * `json` - The JSON value to parse
///
/// # Returns
///
/// `Ok(JsonRpcResponse)` if the input is a valid response.
/// `Err(JsonRpcError)` if validation fails.
///
/// # Validation Rules
///
/// - Must be an object
/// - Must have `jsonrpc` field equal to "2.0"
/// - Must have `id` field
/// - Must have either `result` or `error`, but not both
///
/// # Example
///
/// ```cpp
/// auto json = parse_json(R"({"jsonrpc":"2.0","result":42,"id":1})");
/// auto result = JsonRpcResponse::from_json(unwrap(json));
/// if (is_ok(result)) {
///     auto& resp = unwrap(result);
///     // resp.result->as_i64() == 42
/// }
/// ```
auto JsonRpcResponse::from_json(const JsonValue& json) -> Result<JsonRpcResponse, JsonRpcError> {
    // Must be an object
    if (!json.is_object()) {
        return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                  "Response must be an object");
    }

    // Check jsonrpc version
    auto* version = json.get("jsonrpc");
    if (version == nullptr || !version->is_string() || version->as_string() != "2.0") {
        return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                  "Missing or invalid jsonrpc version");
    }

    // Must have id
    auto* id = json.get("id");
    if (id == nullptr) {
        return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                  "Missing id field");
    }

    auto* result = json.get("result");
    auto* error = json.get("error");

    // Must have either result or error, but not both
    if (result != nullptr && error != nullptr) {
        return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                  "Response cannot have both result and error");
    }

    if (result == nullptr && error == nullptr) {
        return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                  "Response must have either result or error");
    }

    JsonRpcResponse response;
    response.id = id->clone();

    if (result != nullptr) {
        response.result = result->clone();
    } else {
        // Parse error object
        if (!error->is_object()) {
            return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                      "error must be an object");
        }

        auto* code = error->get("code");
        auto* message = error->get("message");

        if (code == nullptr || !code->is_integer()) {
            return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                      "error.code must be an integer");
        }

        if (message == nullptr || !message->is_string()) {
            return JsonRpcError::make(static_cast<int>(JsonRpcErrorCode::InvalidRequest),
                                      "error.message must be a string");
        }

        JsonRpcError err;
        err.code = static_cast<int>(code->as_i64());
        err.message = message->as_string();

        auto* data = error->get("data");
        if (data != nullptr) {
            err.data = data->clone();
        }

        response.error = std::move(err);
    }

    return response;
}

/// Converts this response to a JSON object.
///
/// The resulting object has the structure for success:
/// ```json
/// {
///   "jsonrpc": "2.0",
///   "result": ...,
///   "id": 1
/// }
/// ```
///
/// Or for error:
/// ```json
/// {
///   "jsonrpc": "2.0",
///   "error": {"code": -32600, "message": "..."},
///   "id": 1
/// }
/// ```
///
/// # Returns
///
/// A `JsonValue` object containing the response fields.
auto JsonRpcResponse::to_json() const -> JsonValue {
    JsonObject obj;
    obj["jsonrpc"] = JsonValue(jsonrpc);
    obj["id"] = id.clone();

    if (result.has_value()) {
        obj["result"] = result->clone();
    }

    if (error.has_value()) {
        obj["error"] = error->to_json();
    }

    return JsonValue(std::move(obj));
}

} // namespace tml::json
