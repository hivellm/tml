TML_MODULE("compiler")

//! # JSON Builder Implementation
//!
//! This module implements the fluent JSON builder API.
//! The builder uses a stack to track nested object and array contexts.
//!
//! ## Implementation Details
//!
//! The builder maintains a stack of `Context` objects:
//!
//! - `Object` context: Accumulates key-value pairs
//! - `Array` context: Accumulates values
//!
//! When `end()` is called, the current context is popped and its value
//! is added to the parent context (or becomes the result if at root).
//!
//! ## Nested Object Handling
//!
//! For nested structures like `field_object("key")`:
//!
//! 1. The key is stored in `pending_key`
//! 2. A new object context is pushed
//! 3. When `end()` is called, the object is added to the parent with `pending_key`
//!
//! ## Example Flow
//!
//! ```cpp
//! JsonBuilder().object()              // Push Object context
//!     .field("a", 1)                  // Add to current object
//!     .field_object("nested")         // Store "nested", push new Object
//!         .field("b", 2)              // Add to nested object
//!     .end()                          // Pop nested, add to parent as "nested"
//! .end()                              // Pop root, set as result
//! .build();                           // Return result
//! ```

#include "json/json_builder.hpp"

namespace tml::json {

/// Starts building a new object.
///
/// Pushes a new object context onto the stack.
auto JsonBuilder::object() -> JsonBuilder& {
    Context ctx;
    ctx.kind = Context::Kind::Object;
    ctx.value = json_object();
    stack_.push(std::move(ctx));
    return *this;
}

/// Starts building a new array.
///
/// Pushes a new array context onto the stack.
auto JsonBuilder::array() -> JsonBuilder& {
    Context ctx;
    ctx.kind = Context::Kind::Array;
    ctx.value = json_array();
    stack_.push(std::move(ctx));
    return *this;
}

/// Ends the current object or array context.
///
/// Pops the current context and adds its value to the parent context,
/// or sets it as the final result if at the root level.
auto JsonBuilder::end() -> JsonBuilder& {
    if (stack_.empty()) {
        throw std::runtime_error("JsonBuilder::end() called with empty stack");
    }

    Context ctx = std::move(stack_.top());
    stack_.pop();

    if (stack_.empty()) {
        // Root level - this is the final result
        result_ = std::move(ctx.value);
        has_result_ = true;
    } else {
        // Nested - add to parent
        auto& parent = stack_.top();
        if (parent.kind == Context::Kind::Object) {
            // Add as field with pending key
            if (parent.pending_key.empty()) {
                throw std::runtime_error("JsonBuilder: object ended without pending key");
            }
            parent.value.set(parent.pending_key, std::move(ctx.value));
            parent.pending_key.clear();
        } else {
            // Add as array item
            parent.value.push(std::move(ctx.value));
        }
    }

    return *this;
}

// ============================================================================
// Object Field Methods
// ============================================================================

/// Adds a field with a `JsonValue` to the current object.
auto JsonBuilder::field(const std::string& key, JsonValue value) -> JsonBuilder& {
    if (stack_.empty() || stack_.top().kind != Context::Kind::Object) {
        throw std::runtime_error("JsonBuilder::field() called outside object context");
    }
    stack_.top().value.set(key, std::move(value));
    return *this;
}

/// Adds a string field (C string) to the current object.
auto JsonBuilder::field(const std::string& key, const char* value) -> JsonBuilder& {
    return field(key, JsonValue(value));
}

/// Adds a string field to the current object.
auto JsonBuilder::field(const std::string& key, const std::string& value) -> JsonBuilder& {
    return field(key, JsonValue(value));
}

/// Adds an integer field (int) to the current object.
auto JsonBuilder::field(const std::string& key, int value) -> JsonBuilder& {
    return field(key, JsonValue(static_cast<int64_t>(value)));
}

/// Adds an integer field (int64_t) to the current object.
auto JsonBuilder::field(const std::string& key, int64_t value) -> JsonBuilder& {
    return field(key, JsonValue(value));
}

/// Adds a floating-point field to the current object.
auto JsonBuilder::field(const std::string& key, double value) -> JsonBuilder& {
    return field(key, JsonValue(value));
}

/// Adds a boolean field to the current object.
auto JsonBuilder::field(const std::string& key, bool value) -> JsonBuilder& {
    return field(key, JsonValue(value));
}

/// Adds a null field to the current object.
auto JsonBuilder::field_null(const std::string& key) -> JsonBuilder& {
    return field(key, JsonValue());
}

/// Starts a nested object field.
///
/// Stores the key and pushes a new object context.
auto JsonBuilder::field_object(const std::string& key) -> JsonBuilder& {
    if (stack_.empty() || stack_.top().kind != Context::Kind::Object) {
        throw std::runtime_error("JsonBuilder::field_object() called outside object context");
    }
    stack_.top().pending_key = key;
    return object();
}

/// Starts a nested array field.
///
/// Stores the key and pushes a new array context.
auto JsonBuilder::field_array(const std::string& key) -> JsonBuilder& {
    if (stack_.empty() || stack_.top().kind != Context::Kind::Object) {
        throw std::runtime_error("JsonBuilder::field_array() called outside object context");
    }
    stack_.top().pending_key = key;
    return array();
}

// ============================================================================
// Array Item Methods
// ============================================================================

/// Adds a `JsonValue` item to the current array.
auto JsonBuilder::item(JsonValue value) -> JsonBuilder& {
    if (stack_.empty() || stack_.top().kind != Context::Kind::Array) {
        throw std::runtime_error("JsonBuilder::item() called outside array context");
    }
    stack_.top().value.push(std::move(value));
    return *this;
}

/// Adds a string item (C string) to the current array.
auto JsonBuilder::item(const char* value) -> JsonBuilder& {
    return item(JsonValue(value));
}

/// Adds a string item to the current array.
auto JsonBuilder::item(const std::string& value) -> JsonBuilder& {
    return item(JsonValue(value));
}

/// Adds an integer item (int) to the current array.
auto JsonBuilder::item(int value) -> JsonBuilder& {
    return item(JsonValue(static_cast<int64_t>(value)));
}

/// Adds an integer item (int64_t) to the current array.
auto JsonBuilder::item(int64_t value) -> JsonBuilder& {
    return item(JsonValue(value));
}

/// Adds a floating-point item to the current array.
auto JsonBuilder::item(double value) -> JsonBuilder& {
    return item(JsonValue(value));
}

/// Adds a boolean item to the current array.
auto JsonBuilder::item(bool value) -> JsonBuilder& {
    return item(JsonValue(value));
}

/// Adds a null item to the current array.
auto JsonBuilder::item_null() -> JsonBuilder& {
    return item(JsonValue());
}

/// Starts a nested object item in the current array.
auto JsonBuilder::item_object() -> JsonBuilder& {
    if (stack_.empty() || stack_.top().kind != Context::Kind::Array) {
        throw std::runtime_error("JsonBuilder::item_object() called outside array context");
    }
    return object();
}

/// Starts a nested array item in the current array.
auto JsonBuilder::item_array() -> JsonBuilder& {
    if (stack_.empty() || stack_.top().kind != Context::Kind::Array) {
        throw std::runtime_error("JsonBuilder::item_array() called outside array context");
    }
    return array();
}

// ============================================================================
// Primitive Value Methods
// ============================================================================

/// Sets the result to a null value.
auto JsonBuilder::null() -> JsonBuilder& {
    if (!stack_.empty()) {
        throw std::runtime_error("JsonBuilder::null() called inside object/array context");
    }
    result_ = JsonValue();
    has_result_ = true;
    return *this;
}

/// Sets the result to a boolean value.
auto JsonBuilder::boolean(bool value) -> JsonBuilder& {
    if (!stack_.empty()) {
        throw std::runtime_error("JsonBuilder::boolean() called inside object/array context");
    }
    result_ = JsonValue(value);
    has_result_ = true;
    return *this;
}

/// Sets the result to an integer value.
auto JsonBuilder::integer(int64_t value) -> JsonBuilder& {
    if (!stack_.empty()) {
        throw std::runtime_error("JsonBuilder::integer() called inside object/array context");
    }
    result_ = JsonValue(value);
    has_result_ = true;
    return *this;
}

/// Sets the result to a floating-point value.
auto JsonBuilder::floating(double value) -> JsonBuilder& {
    if (!stack_.empty()) {
        throw std::runtime_error("JsonBuilder::floating() called inside object/array context");
    }
    result_ = JsonValue(value);
    has_result_ = true;
    return *this;
}

/// Sets the result to a string value.
auto JsonBuilder::string(const std::string& value) -> JsonBuilder& {
    if (!stack_.empty()) {
        throw std::runtime_error("JsonBuilder::string() called inside object/array context");
    }
    result_ = JsonValue(value);
    has_result_ = true;
    return *this;
}

// ============================================================================
// Finalization
// ============================================================================

/// Builds and returns the final `JsonValue`.
///
/// # Panics
///
/// Throws if there are unclosed objects/arrays on the stack.
auto JsonBuilder::build() -> JsonValue {
    if (!stack_.empty()) {
        throw std::runtime_error("JsonBuilder::build() called with " +
                                 std::to_string(stack_.size()) + " unclosed context(s)");
    }
    if (!has_result_) {
        return JsonValue(); // Return null if nothing was built
    }
    return std::move(result_);
}

/// Returns `true` if the builder is ready to build.
auto JsonBuilder::is_complete() const -> bool {
    return stack_.empty() && has_result_;
}

} // namespace tml::json
