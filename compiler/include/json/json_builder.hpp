//! # JSON Builder
//!
//! This module provides a fluent API for constructing JSON values programmatically.
//! The builder uses a stack-based approach to handle nested structures.
//!
//! ## Features
//!
//! - **Fluent API**: Chain method calls for readable construction
//! - **Type safety**: Methods are context-aware (object vs array)
//! - **Nested structures**: Support for deeply nested objects and arrays
//! - **Convenience methods**: Typed field/item methods for common types
//!
//! ## Usage Pattern
//!
//! The builder follows this pattern:
//!
//! 1. Start with `object()` or `array()`
//! 2. Add fields (for objects) or items (for arrays)
//! 3. For nested structures, call `object()` or `array()` again
//! 4. Call `end()` to close each nested structure
//! 5. Call `build()` to get the final `JsonValue`
//!
//! ## Example
//!
//! ```cpp
//! #include "json/json_builder.hpp"
//! using namespace tml::json;
//!
//! // Build a complex JSON object
//! auto json = JsonBuilder()
//!     .object()
//!         .field("name", "Alice")
//!         .field("age", 30)
//!         .field_array("tags")
//!             .item("developer")
//!             .item("rust")
//!         .end()
//!         .field_object("address")
//!             .field("city", "Seattle")
//!             .field("zip", "98101")
//!         .end()
//!     .end()
//!     .build();
//!
//! // Result:
//! {"address":{"city":"Seattle","zip":"98101"},"age":30,"name":"Alice","tags":["developer","rust"]}
//! ```

#pragma once

#include "json/json_value.hpp"
#include <stack>
#include <stdexcept>
#include <string>

namespace tml::json {

/// Fluent builder for constructing `JsonValue` objects.
///
/// The builder maintains a stack of contexts to handle nested objects and arrays.
/// Each `object()` or `array()` call pushes a new context, and `end()` pops it.
///
/// # Thread Safety
///
/// The builder is not thread-safe. Each thread should use its own builder instance.
///
/// # Example
///
/// ```cpp
/// auto json = JsonBuilder()
///     .array()
///         .item(1)
///         .item(2)
///         .item_object()
///             .field("nested", true)
///         .end()
///     .end()
///     .build();
/// ```
class JsonBuilder {
public:
    /// Creates a new empty builder.
    JsonBuilder() = default;

    // ========================================================================
    // Structure Methods
    // ========================================================================

    /// Starts building a new object.
    ///
    /// If already inside an object or array, this creates a nested object.
    /// Call `end()` when done adding fields.
    ///
    /// # Returns
    ///
    /// Reference to this builder for chaining.
    auto object() -> JsonBuilder&;

    /// Starts building a new array.
    ///
    /// If already inside an object or array, this creates a nested array.
    /// Call `end()` when done adding items.
    ///
    /// # Returns
    ///
    /// Reference to this builder for chaining.
    auto array() -> JsonBuilder&;

    /// Ends the current object or array and returns to the parent context.
    ///
    /// # Panics
    ///
    /// Throws `std::runtime_error` if called when not inside an object/array.
    ///
    /// # Returns
    ///
    /// Reference to this builder for chaining.
    auto end() -> JsonBuilder&;

    // ========================================================================
    // Object Field Methods
    // ========================================================================

    /// Adds a field with a `JsonValue` to the current object.
    ///
    /// # Arguments
    ///
    /// * `key` - The field name
    /// * `value` - The field value
    ///
    /// # Panics
    ///
    /// Throws if not inside an object context.
    ///
    /// # Returns
    ///
    /// Reference to this builder for chaining.
    auto field(const std::string& key, JsonValue value) -> JsonBuilder&;

    /// Adds a string field to the current object.
    ///
    /// # Arguments
    ///
    /// * `key` - The field name
    /// * `value` - The string value
    auto field(const std::string& key, const char* value) -> JsonBuilder&;

    /// Adds a string field to the current object.
    auto field(const std::string& key, const std::string& value) -> JsonBuilder&;

    /// Adds an integer field to the current object (int).
    auto field(const std::string& key, int value) -> JsonBuilder&;

    /// Adds an integer field to the current object (int64_t).
    auto field(const std::string& key, int64_t value) -> JsonBuilder&;

    /// Adds a floating-point field to the current object.
    auto field(const std::string& key, double value) -> JsonBuilder&;

    /// Adds a boolean field to the current object.
    auto field(const std::string& key, bool value) -> JsonBuilder&;

    /// Adds a null field to the current object.
    auto field_null(const std::string& key) -> JsonBuilder&;

    /// Starts a nested object field.
    ///
    /// Call `end()` when done adding fields to the nested object.
    ///
    /// # Arguments
    ///
    /// * `key` - The field name for the nested object
    auto field_object(const std::string& key) -> JsonBuilder&;

    /// Starts a nested array field.
    ///
    /// Call `end()` when done adding items to the nested array.
    ///
    /// # Arguments
    ///
    /// * `key` - The field name for the nested array
    auto field_array(const std::string& key) -> JsonBuilder&;

    // ========================================================================
    // Array Item Methods
    // ========================================================================

    /// Adds a `JsonValue` item to the current array.
    ///
    /// # Arguments
    ///
    /// * `value` - The item value
    ///
    /// # Panics
    ///
    /// Throws if not inside an array context.
    ///
    /// # Returns
    ///
    /// Reference to this builder for chaining.
    auto item(JsonValue value) -> JsonBuilder&;

    /// Adds a string item to the current array.
    auto item(const char* value) -> JsonBuilder&;

    /// Adds a string item to the current array.
    auto item(const std::string& value) -> JsonBuilder&;

    /// Adds an integer item to the current array (int).
    auto item(int value) -> JsonBuilder&;

    /// Adds an integer item to the current array (int64_t).
    auto item(int64_t value) -> JsonBuilder&;

    /// Adds a floating-point item to the current array.
    auto item(double value) -> JsonBuilder&;

    /// Adds a boolean item to the current array.
    auto item(bool value) -> JsonBuilder&;

    /// Adds a null item to the current array.
    auto item_null() -> JsonBuilder&;

    /// Starts a nested object item in the current array.
    ///
    /// Call `end()` when done adding fields to the nested object.
    auto item_object() -> JsonBuilder&;

    /// Starts a nested array item in the current array.
    ///
    /// Call `end()` when done adding items to the nested array.
    auto item_array() -> JsonBuilder&;

    // ========================================================================
    // Primitive Value Methods
    // ========================================================================

    /// Sets the result to a null value.
    ///
    /// Use this when building a standalone primitive value.
    auto null() -> JsonBuilder&;

    /// Sets the result to a boolean value.
    ///
    /// Use this when building a standalone primitive value.
    auto boolean(bool value) -> JsonBuilder&;

    /// Sets the result to an integer value.
    ///
    /// Use this when building a standalone primitive value.
    auto integer(int64_t value) -> JsonBuilder&;

    /// Sets the result to a floating-point value.
    ///
    /// Use this when building a standalone primitive value.
    auto floating(double value) -> JsonBuilder&;

    /// Sets the result to a string value.
    ///
    /// Use this when building a standalone primitive value.
    auto string(const std::string& value) -> JsonBuilder&;

    // ========================================================================
    // Finalization
    // ========================================================================

    /// Builds and returns the final `JsonValue`.
    ///
    /// # Panics
    ///
    /// Throws `std::runtime_error` if there are unclosed objects/arrays.
    ///
    /// # Returns
    ///
    /// The constructed `JsonValue`.
    [[nodiscard]] auto build() -> JsonValue;

    /// Returns `true` if the builder is ready to build (no unclosed contexts).
    [[nodiscard]] auto is_complete() const -> bool;

private:
    /// Context for tracking nested structures.
    struct Context {
        enum class Kind { Object, Array };
        Kind kind;
        JsonValue value;
        std::string pending_key; ///< For objects: key waiting for a value
    };

    std::stack<Context> stack_;
    JsonValue result_;
    bool has_result_ = false;
};

} // namespace tml::json
