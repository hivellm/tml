TML_MODULE("mcp")

//! # MCP Types Implementation
//!
//! Serialization implementations for MCP protocol types.

#include "mcp/mcp_types.hpp"

namespace tml::mcp {

// ============================================================================
// ServerInfo
// ============================================================================

auto ServerInfo::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj["name"] = json::JsonValue(name);
    obj["version"] = json::JsonValue(version);
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// ClientInfo
// ============================================================================

auto ClientInfo::from_json(const json::JsonValue& json) -> std::optional<ClientInfo> {
    if (!json.is_object()) {
        return std::nullopt;
    }

    auto* name = json.get("name");
    auto* version = json.get("version");

    if (name == nullptr || !name->is_string()) {
        return std::nullopt;
    }

    ClientInfo info;
    info.name = name->as_string();
    info.version = (version != nullptr && version->is_string()) ? version->as_string() : "unknown";
    return info;
}

// ============================================================================
// ToolParameter
// ============================================================================

auto ToolParameter::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj["type"] = json::JsonValue(type);
    obj["description"] = json::JsonValue(description);
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// Tool
// ============================================================================

auto Tool::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj["name"] = json::JsonValue(name);
    obj["description"] = json::JsonValue(description);

    // Build inputSchema
    json::JsonObject schema;
    schema["type"] = json::JsonValue("object");

    json::JsonObject properties;
    json::JsonArray required_params;

    for (const auto& param : parameters) {
        properties[param.name] = param.to_json();
        if (param.required) {
            required_params.push_back(json::JsonValue(param.name));
        }
    }

    schema["properties"] = json::JsonValue(std::move(properties));
    if (!required_params.empty()) {
        schema["required"] = json::JsonValue(std::move(required_params));
    }

    obj["inputSchema"] = json::JsonValue(std::move(schema));
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// Resource
// ============================================================================

auto Resource::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj["uri"] = json::JsonValue(uri);
    obj["name"] = json::JsonValue(name);
    obj["description"] = json::JsonValue(description);
    obj["mimeType"] = json::JsonValue(mime_type);
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// ServerCapabilities
// ============================================================================

auto ServerCapabilities::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    if (tools) {
        json::JsonObject tools_cap;
        obj["tools"] = json::JsonValue(std::move(tools_cap));
    }
    if (resources) {
        json::JsonObject resources_cap;
        obj["resources"] = json::JsonValue(std::move(resources_cap));
    }
    if (prompts) {
        json::JsonObject prompts_cap;
        obj["prompts"] = json::JsonValue(std::move(prompts_cap));
    }
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// ToolContent
// ============================================================================

auto ToolContent::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj["type"] = json::JsonValue(type);
    obj["text"] = json::JsonValue(text);
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// ToolResult
// ============================================================================

auto ToolResult::to_json() const -> json::JsonValue {
    json::JsonObject obj;

    json::JsonArray content_arr;
    for (const auto& c : content) {
        content_arr.push_back(c.to_json());
    }
    obj["content"] = json::JsonValue(std::move(content_arr));

    if (is_error) {
        obj["isError"] = json::JsonValue(true);
    }

    return json::JsonValue(std::move(obj));
}

auto ToolResult::text(const std::string& text) -> ToolResult {
    ToolResult result;
    result.content.push_back(ToolContent{"text", text});
    return result;
}

auto ToolResult::error(const std::string& message) -> ToolResult {
    ToolResult result;
    result.content.push_back(ToolContent{"text", message});
    result.is_error = true;
    return result;
}

} // namespace tml::mcp
