//! # MCP Types
//!
//! Core types for Model Context Protocol (MCP) integration.
//!
//! ## Overview
//!
//! MCP enables standardized communication between AI models and development tools.
//! This module defines the core protocol types:
//!
//! - **ServerInfo**: Server identity and version
//! - **ClientInfo**: Client identity and version
//! - **ServerCapabilities**: Features the server supports
//! - **Tool**: Tool definition with JSON schema
//! - **Resource**: Resource definition
//!
//! ## Protocol Version
//!
//! This implementation targets MCP protocol version 2025-03-26.

#pragma once

#include "json/json_value.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tml::mcp {

/// MCP protocol version this implementation supports.
constexpr const char* MCP_PROTOCOL_VERSION = "2025-03-26";

/// Server identity information.
///
/// Sent during initialization to identify the server.
struct ServerInfo {
    std::string name;    ///< Server name (e.g., "tml-compiler")
    std::string version; ///< Server version (e.g., "0.1.0")

    [[nodiscard]] auto to_json() const -> json::JsonValue;
};

/// Client identity information.
///
/// Received during initialization to identify the client.
struct ClientInfo {
    std::string name;    ///< Client name (e.g., "claude-code")
    std::string version; ///< Client version

    [[nodiscard]] static auto from_json(const json::JsonValue& json)
        -> std::optional<ClientInfo>;
};

/// Tool parameter schema.
///
/// Defines a single parameter for a tool using JSON Schema.
struct ToolParameter {
    std::string name;        ///< Parameter name
    std::string type;        ///< JSON Schema type (string, number, boolean, etc.)
    std::string description; ///< Parameter description
    bool required = true;    ///< Whether the parameter is required

    [[nodiscard]] auto to_json() const -> json::JsonValue;
};

/// Tool definition.
///
/// Describes a callable tool with its parameters and behavior.
///
/// # Example
///
/// ```cpp
/// Tool compile_tool{
///     .name = "compile",
///     .description = "Compile a TML source file",
///     .parameters = {
///         {"file", "string", "Path to the source file", true},
///         {"optimize", "string", "Optimization level (O0-O3)", false},
///     }
/// };
/// ```
struct Tool {
    std::string name;                    ///< Tool name (e.g., "compile")
    std::string description;             ///< Human-readable description
    std::vector<ToolParameter> parameters; ///< Input parameters

    [[nodiscard]] auto to_json() const -> json::JsonValue;
};

/// Resource definition.
///
/// Describes an accessible resource (file, documentation, etc.).
struct Resource {
    std::string uri;         ///< Resource URI (e.g., "file:///path/to/file.tml")
    std::string name;        ///< Human-readable name
    std::string description; ///< Resource description
    std::string mime_type;   ///< MIME type (e.g., "text/plain")

    [[nodiscard]] auto to_json() const -> json::JsonValue;
};

/// Server capabilities.
///
/// Declares what features the server supports.
struct ServerCapabilities {
    bool tools = true;      ///< Server supports tools
    bool resources = false; ///< Server supports resources
    bool prompts = false;   ///< Server supports prompts

    [[nodiscard]] auto to_json() const -> json::JsonValue;
};

/// Tool call result content.
///
/// Represents the result of a tool invocation.
struct ToolContent {
    std::string type = "text"; ///< Content type ("text", "image", "resource")
    std::string text;          ///< Text content (for type="text")

    [[nodiscard]] auto to_json() const -> json::JsonValue;
};

/// Tool call result.
///
/// The complete result of a tool invocation.
struct ToolResult {
    std::vector<ToolContent> content; ///< Result content
    bool is_error = false;            ///< Whether this is an error result

    [[nodiscard]] auto to_json() const -> json::JsonValue;

    /// Creates a text result.
    static auto text(const std::string& text) -> ToolResult;

    /// Creates an error result.
    static auto error(const std::string& message) -> ToolResult;
};

} // namespace tml::mcp
