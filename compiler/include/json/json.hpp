//! # TML JSON Library
//!
//! This is the main public header for the TML JSON library. It provides a complete
//! JSON implementation with parsing, serialization, and JSON-RPC 2.0 support.
//!
//! ## Features
//!
//! - **Zero-copy parsing**: Uses `std::string_view` to minimize allocations
//! - **Integer preservation**: Numbers are stored as Int64/Uint64/Double
//! - **Fluent builder API**: Construct JSON with a chainable API
//! - **JSON-RPC 2.0**: Full support for MCP integration
//! - **Pretty printing**: Configurable indentation for output
//!
//! ## Quick Start
//!
//! ```cpp
//! #include "json/json.hpp"
//! using namespace tml::json;
//!
//! // Parse JSON
//! auto result = parse_json(R"({"name": "Alice", "age": 30})");
//! if (is_ok(result)) {
//!     auto& json = unwrap(result);
//!     std::cout << json.get("name")->as_string() << std::endl;
//! }
//!
//! // Build JSON
//! auto json = JsonBuilder()
//!     .object()
//!         .field("name", "Bob")
//!         .field("scores", JsonBuilder().array().item(95).item(87).end().build())
//!     .end()
//!     .build();
//! std::cout << json.to_string_pretty(2) << std::endl;
//! ```
//!
//! ## Modules
//!
//! | Header | Description |
//! |--------|-------------|
//! | `json_error.hpp` | Error type with location information |
//! | `json_value.hpp` | Core JSON types (`JsonValue`, `JsonNumber`) |
//! | `json_parser.hpp` | Lexer and parser for JSON input |
//! | `json_builder.hpp` | Fluent builder API |
//! | `json_rpc.hpp` | JSON-RPC 2.0 request/response types |
//!
//! ## Type Summary
//!
//! | Type | Purpose |
//! |------|---------|
//! | `JsonValue` | Variant type representing any JSON value |
//! | `JsonNumber` | Discriminated union for numeric precision |
//! | `JsonArray` | Vector of `JsonValue` |
//! | `JsonObject` | Ordered map of string to `JsonValue` |
//! | `JsonError` | Parse error with line/column |
//! | `JsonBuilder` | Fluent API for constructing JSON |
//! | `JsonRpcRequest` | JSON-RPC 2.0 request |
//! | `JsonRpcResponse` | JSON-RPC 2.0 response |
//!
//! ## Error Handling
//!
//! All fallible operations return `Result<T, E>`. Use the helper functions:
//!
//! ```cpp
//! auto result = parse_json(input);
//! if (is_ok(result)) {
//!     auto& value = unwrap(result);
//!     // Use value...
//! } else {
//!     auto& error = unwrap_err(result);
//!     std::cerr << error.to_string() << std::endl;
//! }
//! ```

#pragma once

// Core includes
#include "json/json_error.hpp"
#include "json/json_value.hpp"

// Parsing
#include "json/json_parser.hpp"
#include "json/json_fast_parser.hpp"

// Building
#include "json/json_builder.hpp"

// JSON-RPC 2.0
#include "json/json_rpc.hpp"

// Schema validation
#include "json/json_schema.hpp"
