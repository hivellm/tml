//! # JSON Schema Validation
//!
//! This module provides basic schema validation for JSON values.
//! It supports type checking, required fields, and array element validation.
//!
//! ## Features
//!
//! - **Type validation**: Check if a value has the expected JSON type
//! - **Required fields**: Verify objects contain required keys
//! - **Array validation**: Validate all array elements match a type
//! - **Nested validation**: Validate nested structures recursively
//!
//! ## Example
//!
//! ```cpp
//! #include "json/json_schema.hpp"
//! using namespace tml::json;
//!
//! // Define a schema for a user object
//! auto schema = JsonSchema::object()
//!     .required("name", JsonSchema::string())
//!     .required("age", JsonSchema::integer())
//!     .optional("email", JsonSchema::string());
//!
//! // Validate a JSON value
//! auto result = schema.validate(user_json);
//! if (!result.valid) {
//!     std::cerr << "Validation error: " << result.error << std::endl;
//! }
//! ```

#pragma once

#include "json/json_value.hpp"
#include <string>
#include <vector>

namespace tml::json {

/// Result of schema validation.
struct ValidationResult {
    /// Whether the validation passed.
    bool valid = true;

    /// Error message if validation failed.
    std::string error;

    /// Path to the invalid value (e.g., "users[0].name").
    std::string path;

    /// Creates a successful validation result.
    static auto ok() -> ValidationResult {
        return ValidationResult{true, "", ""};
    }

    /// Creates a failed validation result.
    ///
    /// # Arguments
    ///
    /// * `error` - Description of the validation error
    /// * `path` - JSON path to the invalid value
    static auto fail(const std::string& error, const std::string& path = "") -> ValidationResult {
        return ValidationResult{false, error, path};
    }
};

/// Schema for validating JSON values.
///
/// Schemas are built using factory methods and can be composed
/// to validate complex nested structures.
class JsonSchema {
public:
    /// The expected JSON type.
    enum class Type {
        Any,     ///< Accept any type
        Null,    ///< Expect null
        Bool,    ///< Expect boolean
        Integer, ///< Expect integer (Int64 or Uint64)
        Number,  ///< Expect any number
        String,  ///< Expect string
        Array,   ///< Expect array
        Object   ///< Expect object
    };

    /// Field requirement for object schemas.
    struct FieldSchema {
        std::string name;
        std::unique_ptr<JsonSchema> schema;
        bool required;
    };

    /// Creates a schema that accepts any type.
    static auto any() -> JsonSchema;

    /// Creates a schema that expects null.
    static auto null() -> JsonSchema;

    /// Creates a schema that expects a boolean.
    static auto boolean() -> JsonSchema;

    /// Creates a schema that expects an integer.
    static auto integer() -> JsonSchema;

    /// Creates a schema that expects any number.
    static auto number() -> JsonSchema;

    /// Creates a schema that expects a string.
    static auto string() -> JsonSchema;

    /// Creates a schema that expects an array.
    ///
    /// By default, accepts arrays with any element types.
    static auto array() -> JsonSchema;

    /// Creates a schema that expects an array with elements of a specific type.
    ///
    /// # Arguments
    ///
    /// * `element_schema` - Schema for array elements
    static auto array_of(JsonSchema element_schema) -> JsonSchema;

    /// Creates a schema that expects an object.
    ///
    /// By default, accepts objects with any fields.
    static auto object() -> JsonSchema;

    /// Adds a required field to an object schema.
    ///
    /// # Arguments
    ///
    /// * `name` - Field name
    /// * `schema` - Schema for the field value
    ///
    /// # Returns
    ///
    /// Rvalue reference for move-based chaining.
    auto required(const std::string& name, JsonSchema schema) -> JsonSchema&&;

    /// Adds an optional field to an object schema.
    ///
    /// # Arguments
    ///
    /// * `name` - Field name
    /// * `schema` - Schema for the field value (if present)
    ///
    /// # Returns
    ///
    /// Rvalue reference for move-based chaining.
    auto optional(const std::string& name, JsonSchema schema) -> JsonSchema&&;

    /// Validates a JSON value against this schema.
    ///
    /// # Arguments
    ///
    /// * `value` - The value to validate
    ///
    /// # Returns
    ///
    /// A `ValidationResult` indicating success or failure.
    [[nodiscard]] auto validate(const JsonValue& value) const -> ValidationResult;

    /// Validates a JSON value with a path prefix.
    ///
    /// # Arguments
    ///
    /// * `value` - The value to validate
    /// * `path` - Current path for error reporting
    ///
    /// # Returns
    ///
    /// A `ValidationResult` indicating success or failure.
    [[nodiscard]] auto validate(const JsonValue& value, const std::string& path) const
        -> ValidationResult;

    /// Default constructor (creates Any schema).
    JsonSchema() = default;

    /// Move constructor.
    JsonSchema(JsonSchema&&) noexcept = default;

    /// Move assignment.
    auto operator=(JsonSchema&&) noexcept -> JsonSchema& = default;

    /// Copy is deleted (use move semantics).
    JsonSchema(const JsonSchema&) = delete;
    auto operator=(const JsonSchema&) -> JsonSchema& = delete;

private:
    Type type_ = Type::Any;
    std::vector<FieldSchema> fields_;
    std::unique_ptr<JsonSchema> element_schema_;

    explicit JsonSchema(Type type) : type_(type) {}
};

} // namespace tml::json
