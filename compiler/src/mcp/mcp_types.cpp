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
    obj.reserve(2);
    obj.emplace_back("name", json::JsonValue(name));
    obj.emplace_back("version", json::JsonValue(version));
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
    obj.reserve(2);
    obj.emplace_back("type", json::JsonValue(type));
    obj.emplace_back("description", json::JsonValue(description));
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// Tool
// ============================================================================

auto Tool::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj.reserve(3);
    obj.emplace_back("name", json::JsonValue(name));
    obj.emplace_back("description", json::JsonValue(description));

    // Build inputSchema
    json::JsonObject schema;
    schema.reserve(3);
    schema.emplace_back("type", json::JsonValue("object"));

    json::JsonObject properties;
    properties.reserve(parameters.size());
    json::JsonArray required_params;

    for (const auto& param : parameters) {
        properties.emplace_back(param.name, param.to_json());
        if (param.required) {
            required_params.push_back(json::JsonValue(param.name));
        }
    }

    schema.emplace_back("properties", json::JsonValue(std::move(properties)));
    if (!required_params.empty()) {
        schema.emplace_back("required", json::JsonValue(std::move(required_params)));
    }

    obj.emplace_back("inputSchema", json::JsonValue(std::move(schema)));
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// Resource
// ============================================================================

auto Resource::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj.reserve(4);
    obj.emplace_back("uri", json::JsonValue(uri));
    obj.emplace_back("name", json::JsonValue(name));
    obj.emplace_back("description", json::JsonValue(description));
    obj.emplace_back("mimeType", json::JsonValue(mime_type));
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// ServerCapabilities
// ============================================================================

auto ServerCapabilities::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    if (tools) {
        json::JsonObject tools_cap;
        obj.emplace_back("tools", json::JsonValue(std::move(tools_cap)));
    }
    if (resources) {
        json::JsonObject resources_cap;
        obj.emplace_back("resources", json::JsonValue(std::move(resources_cap)));
    }
    if (prompts) {
        json::JsonObject prompts_cap;
        obj.emplace_back("prompts", json::JsonValue(std::move(prompts_cap)));
    }
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// ToolContent
// ============================================================================

auto ToolContent::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj.reserve(2);
    obj.emplace_back("type", json::JsonValue(type));
    obj.emplace_back("text", json::JsonValue(text));
    return json::JsonValue(std::move(obj));
}

// ============================================================================
// ToolResult
// ============================================================================

auto ToolResult::to_json() const -> json::JsonValue {
    json::JsonObject obj;
    obj.reserve(2);

    json::JsonArray content_arr;
    for (const auto& c : content) {
        content_arr.push_back(c.to_json());
    }
    obj.emplace_back("content", json::JsonValue(std::move(content_arr)));

    if (is_error) {
        obj.emplace_back("isError", json::JsonValue(true));
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
