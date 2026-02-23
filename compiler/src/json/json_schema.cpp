TML_MODULE("compiler")

//! # JSON Schema Validation Implementation
//!
//! This module implements basic JSON schema validation.

#include "json/json_schema.hpp"

namespace tml::json {

// ============================================================================
// Factory Methods
// ============================================================================

auto JsonSchema::any() -> JsonSchema {
    return JsonSchema(Type::Any);
}

auto JsonSchema::null() -> JsonSchema {
    return JsonSchema(Type::Null);
}

auto JsonSchema::boolean() -> JsonSchema {
    return JsonSchema(Type::Bool);
}

auto JsonSchema::integer() -> JsonSchema {
    return JsonSchema(Type::Integer);
}

auto JsonSchema::number() -> JsonSchema {
    return JsonSchema(Type::Number);
}

auto JsonSchema::string() -> JsonSchema {
    return JsonSchema(Type::String);
}

auto JsonSchema::array() -> JsonSchema {
    return JsonSchema(Type::Array);
}

auto JsonSchema::array_of(JsonSchema element_schema) -> JsonSchema {
    JsonSchema schema(Type::Array);
    schema.element_schema_ = std::make_unique<JsonSchema>(std::move(element_schema));
    return schema;
}

auto JsonSchema::object() -> JsonSchema {
    return JsonSchema(Type::Object);
}

// ============================================================================
// Builder Methods
// ============================================================================

auto JsonSchema::required(const std::string& name, JsonSchema schema) -> JsonSchema&& {
    fields_.push_back(FieldSchema{name, std::make_unique<JsonSchema>(std::move(schema)), true});
    return std::move(*this);
}

auto JsonSchema::optional(const std::string& name, JsonSchema schema) -> JsonSchema&& {
    fields_.push_back(FieldSchema{name, std::make_unique<JsonSchema>(std::move(schema)), false});
    return std::move(*this);
}

// ============================================================================
// Validation
// ============================================================================

namespace {

/// Returns a human-readable name for a JSON type.
auto type_name(const JsonValue& value) -> std::string {
    if (value.is_null())
        return "null";
    if (value.is_bool())
        return "boolean";
    if (value.is_integer())
        return "integer";
    if (value.is_number())
        return "number";
    if (value.is_string())
        return "string";
    if (value.is_array())
        return "array";
    if (value.is_object())
        return "object";
    return "unknown";
}

/// Returns a human-readable name for a schema type.
auto schema_type_name(JsonSchema::Type type) -> std::string {
    switch (type) {
    case JsonSchema::Type::Any:
        return "any";
    case JsonSchema::Type::Null:
        return "null";
    case JsonSchema::Type::Bool:
        return "boolean";
    case JsonSchema::Type::Integer:
        return "integer";
    case JsonSchema::Type::Number:
        return "number";
    case JsonSchema::Type::String:
        return "string";
    case JsonSchema::Type::Array:
        return "array";
    case JsonSchema::Type::Object:
        return "object";
    }
    return "unknown";
}

} // anonymous namespace

auto JsonSchema::validate(const JsonValue& value) const -> ValidationResult {
    return validate(value, "");
}

auto JsonSchema::validate(const JsonValue& value, const std::string& path) const
    -> ValidationResult {

    // Type checking
    bool type_ok = false;
    switch (type_) {
    case Type::Any:
        type_ok = true;
        break;
    case Type::Null:
        type_ok = value.is_null();
        break;
    case Type::Bool:
        type_ok = value.is_bool();
        break;
    case Type::Integer:
        type_ok = value.is_integer();
        break;
    case Type::Number:
        type_ok = value.is_number();
        break;
    case Type::String:
        type_ok = value.is_string();
        break;
    case Type::Array:
        type_ok = value.is_array();
        break;
    case Type::Object:
        type_ok = value.is_object();
        break;
    }

    if (!type_ok) {
        return ValidationResult::fail(
            "expected " + schema_type_name(type_) + ", got " + type_name(value), path);
    }

    // Array element validation
    if (type_ == Type::Array && element_schema_ && value.is_array()) {
        const auto& arr = value.as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            std::string elem_path =
                path.empty() ? "[" + std::to_string(i) + "]" : path + "[" + std::to_string(i) + "]";
            auto result = element_schema_->validate(arr[i], elem_path);
            if (!result.valid) {
                return result;
            }
        }
    }

    // Object field validation
    if (type_ == Type::Object && !fields_.empty() && value.is_object()) {
        for (const auto& field : fields_) {
            std::string field_path = path.empty() ? field.name : path + "." + field.name;

            auto* field_value = value.get(field.name);

            if (field_value == nullptr) {
                if (field.required) {
                    return ValidationResult::fail("missing required field '" + field.name + "'",
                                                  path);
                }
                // Optional field not present, skip validation
                continue;
            }

            auto result = field.schema->validate(*field_value, field_path);
            if (!result.valid) {
                return result;
            }
        }
    }

    return ValidationResult::ok();
}

} // namespace tml::json
