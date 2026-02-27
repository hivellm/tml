# Specification: JSON-RPC Types

## 1. Request

```cpp
struct JsonRpcRequest {
    std::string jsonrpc = "2.0";
    std::string method;
    std::optional<std::unique_ptr<JsonValue>> params;
    std::variant<std::string, int64_t, std::nullptr_t> id;

    static auto parse(const JsonValue& json) -> Result<JsonRpcRequest, JsonError>;
    auto to_json() const -> std::unique_ptr<JsonValue>;
};
```

## 2. Response

```cpp
struct JsonRpcResponse {
    std::string jsonrpc = "2.0";
    std::optional<std::unique_ptr<JsonValue>> result;
    std::optional<JsonRpcError> error;
    std::variant<std::string, int64_t, std::nullptr_t> id;

    static auto success(std::variant<std::string, int64_t, std::nullptr_t> id,
                        std::unique_ptr<JsonValue> result) -> JsonRpcResponse;
    static auto error(std::variant<std::string, int64_t, std::nullptr_t> id,
                      JsonRpcError error) -> JsonRpcResponse;
    auto to_json() const -> std::unique_ptr<JsonValue>;
};
```

## 3. Error

```cpp
struct JsonRpcError {
    int code;
    std::string message;
    std::optional<std::unique_ptr<JsonValue>> data;

    // Standard error codes
    static constexpr int PARSE_ERROR = -32700;
    static constexpr int INVALID_REQUEST = -32600;
    static constexpr int METHOD_NOT_FOUND = -32601;
    static constexpr int INVALID_PARAMS = -32602;
    static constexpr int INTERNAL_ERROR = -32603;
};
```

## 4. Batch Processing

JSON-RPC 2.0 supports batch requests:

```cpp
auto parse_batch(const JsonValue& json)
    -> Result<std::vector<JsonRpcRequest>, JsonError>;

auto batch_response(std::vector<JsonRpcResponse> responses)
    -> std::unique_ptr<JsonValue>;
```

## 5. MCP Integration Points

For MCP (Model Context Protocol):

- `initialize` method for capability negotiation
- `tools/list` to enumerate available tools
- `tools/call` to invoke a tool
- `resources/list` to enumerate resources
- `resources/read` to read a resource
- `prompts/list` to enumerate prompts
- `prompts/get` to get a prompt
