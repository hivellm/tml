//! # JSON Value Types
//!
//! This module provides the core JSON value types for the TML JSON library.
//! It includes `JsonNumber` for precise number representation and `JsonValue`
//! as a variant type for all JSON values.
//!
//! ## Features
//!
//! - **Integer precision**: Numbers without decimals are stored as `int64_t` or `uint64_t`
//! - **Type discrimination**: Query and access values by their JSON type
//! - **Value semantics**: `JsonValue` can be copied, moved, and compared
//! - **Recursive structures**: Arrays and objects can contain nested values
//! - **Factory functions**: Convenient `json_*()` functions for creating values
//!
//! ## Number Handling
//!
//! JSON numbers are stored with type discrimination to preserve precision:
//!
//! | JSON Input | Storage Type | Reason |
//! |------------|--------------|--------|
//! | `42` | `Int64` | No decimal point |
//! | `18446744073709551615` | `Uint64` | Too large for int64 |
//! | `3.14` | `Double` | Has decimal point |
//! | `1e10` | `Double` | Has exponent |
//!
//! ## Example
//!
//! ```cpp
//! #include "json/json_value.hpp"
//! using namespace tml::json;
//!
//! // Create values using factory functions
//! auto null_val = json_null();
//! auto bool_val = json_bool(true);
//! auto int_val = json_int(42);
//! auto str_val = json_string("hello");
//!
//! // Create from constructors
//! JsonValue obj(JsonObject{{"name", JsonValue("Alice")}, {"age", JsonValue(30)}});
//!
//! // Type queries
//! if (obj.is_object()) {
//!     auto* name = obj.get("name");
//!     if (name && name->is_string()) {
//!         std::cout << name->as_string() << std::endl;
//!     }
//! }
//!
//! // Integer precision is preserved
//! JsonValue id(9007199254740993LL);  // > 2^53
//! assert(id.is_integer());
//! assert(id.as_i64() == 9007199254740993LL);
//! ```

#pragma once

#include "common.hpp"

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tml::json {

// ============================================================================
// Forward Declarations and Type Aliases
// ============================================================================

struct JsonValue;

/// A JSON array containing ordered values.
using JsonArray = std::vector<JsonValue>;

/// A JSON object containing key-value pairs (ordered by key).
using JsonObject = std::map<std::string, JsonValue>;

// ============================================================================
// JsonNumber
// ============================================================================

/// Discriminated union for JSON numbers preserving integer precision.
///
/// JSON numbers are stored in their most precise representation:
/// - Integers without decimals/exponents are stored as `Int64` or `Uint64`
/// - Numbers with decimals or exponents are stored as `Double`
///
/// This preserves the distinction between `42` and `42.0` which is important
/// for IDs, counts, and other integer values in JSON-RPC and MCP protocols.
///
/// # Storage Strategy
///
/// The parser determines the storage type based on the number's format:
/// 1. No decimal point and no exponent → try integer
/// 2. Value fits in `int64_t` → `Int64`
/// 3. Value positive and fits in `uint64_t` → `Uint64`
/// 4. Otherwise → `Double`
///
/// # Example
///
/// ```cpp
/// JsonNumber i(42);           // Int64
/// JsonNumber u(UINT64_MAX);   // Uint64
/// JsonNumber f(3.14);         // Double
///
/// // Type queries
/// assert(i.is_integer());
/// assert(i.is_signed());
/// assert(!f.is_integer());
///
/// // Safe conversion
/// if (auto val = i.try_as_i32()) {
///     int32_t x = *val;  // Safe, no overflow
/// }
///
/// // Lossy conversion (always available)
/// double d = i.as_f64();  // May lose precision for large integers
/// ```
struct JsonNumber {
    /// The storage kind for this number.
    ///
    /// Determines which union field is active.
    enum class Kind : uint8_t {
        Int64,  ///< Signed 64-bit integer (`i64` field)
        Uint64, ///< Unsigned 64-bit integer (`u64` field)
        Double  ///< IEEE 754 double precision float (`f64` field)
    };

    /// The storage kind for this number.
    Kind kind;

    /// The number value (only one field is active based on `kind`).
    union {
        int64_t i64;  ///< Active when `kind == Kind::Int64`
        uint64_t u64; ///< Active when `kind == Kind::Uint64`
        double f64;   ///< Active when `kind == Kind::Double`
    };

    // ========================================================================
    // Constructors
    // ========================================================================

    /// Constructs a number from a signed 64-bit integer.
    ///
    /// # Arguments
    ///
    /// * `value` - The integer value
    explicit JsonNumber(int64_t value) : kind(Kind::Int64), i64(value) {}

    /// Constructs a number from an unsigned 64-bit integer.
    ///
    /// # Arguments
    ///
    /// * `value` - The unsigned integer value
    explicit JsonNumber(uint64_t value) : kind(Kind::Uint64), u64(value) {}

    /// Constructs a number from a double.
    ///
    /// # Arguments
    ///
    /// * `value` - The floating-point value
    explicit JsonNumber(double value) : kind(Kind::Double), f64(value) {}

    /// Default constructor creates zero as `Int64`.
    JsonNumber() : kind(Kind::Int64), i64(0) {}

    // ========================================================================
    // Type Queries
    // ========================================================================

    /// Returns `true` if this is an integer (`Int64` or `Uint64`).
    ///
    /// Use this to check if the number can be accessed without precision loss
    /// via `try_as_i64()` or `try_as_u64()`.
    [[nodiscard]] auto is_integer() const -> bool {
        return kind != Kind::Double;
    }

    /// Returns `true` if this is a signed integer (`Int64`).
    [[nodiscard]] auto is_signed() const -> bool {
        return kind == Kind::Int64;
    }

    /// Returns `true` if this is an unsigned integer (`Uint64`).
    [[nodiscard]] auto is_unsigned() const -> bool {
        return kind == Kind::Uint64;
    }

    /// Returns `true` if this is a floating-point number (`Double`).
    [[nodiscard]] auto is_float() const -> bool {
        return kind == Kind::Double;
    }

    // ========================================================================
    // Safe Accessors
    // ========================================================================

    /// Attempts to get the value as `int64_t`.
    ///
    /// Returns `std::nullopt` if:
    /// - The value is a `Double` (use `as_f64()` instead)
    /// - The value is a `Uint64` larger than `INT64_MAX`
    ///
    /// # Returns
    ///
    /// The value as `int64_t` if conversion is lossless, otherwise `std::nullopt`.
    [[nodiscard]] auto try_as_i64() const -> std::optional<int64_t> {
        switch (kind) {
        case Kind::Int64:
            return i64;
        case Kind::Uint64:
            if (u64 <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                return static_cast<int64_t>(u64);
            }
            return std::nullopt;
        case Kind::Double:
            return std::nullopt;
        }
        return std::nullopt;
    }

    /// Attempts to get the value as `uint64_t`.
    ///
    /// Returns `std::nullopt` if:
    /// - The value is a `Double`
    /// - The value is a negative `Int64`
    ///
    /// # Returns
    ///
    /// The value as `uint64_t` if conversion is lossless, otherwise `std::nullopt`.
    [[nodiscard]] auto try_as_u64() const -> std::optional<uint64_t> {
        switch (kind) {
        case Kind::Int64:
            if (i64 >= 0) {
                return static_cast<uint64_t>(i64);
            }
            return std::nullopt;
        case Kind::Uint64:
            return u64;
        case Kind::Double:
            return std::nullopt;
        }
        return std::nullopt;
    }

    /// Attempts to get the value as `int32_t`.
    ///
    /// Returns `std::nullopt` if the value is out of `int32_t` range or is a float.
    [[nodiscard]] auto try_as_i32() const -> std::optional<int32_t> {
        auto val = try_as_i64();
        if (val && *val >= std::numeric_limits<int32_t>::min() &&
            *val <= std::numeric_limits<int32_t>::max()) {
            return static_cast<int32_t>(*val);
        }
        return std::nullopt;
    }

    /// Attempts to get the value as `uint32_t`.
    ///
    /// Returns `std::nullopt` if the value is out of `uint32_t` range or negative.
    [[nodiscard]] auto try_as_u32() const -> std::optional<uint32_t> {
        auto val = try_as_u64();
        if (val && *val <= std::numeric_limits<uint32_t>::max()) {
            return static_cast<uint32_t>(*val);
        }
        return std::nullopt;
    }

    // ========================================================================
    // Lossy Accessor
    // ========================================================================

    /// Gets the value as `double`.
    ///
    /// This conversion always succeeds but may lose precision for large integers
    /// (values larger than 2^53 may not round-trip correctly).
    ///
    /// # Returns
    ///
    /// The value converted to `double`.
    [[nodiscard]] auto as_f64() const -> double {
        switch (kind) {
        case Kind::Int64:
            return static_cast<double>(i64);
        case Kind::Uint64:
            return static_cast<double>(u64);
        case Kind::Double:
            return f64;
        }
        return 0.0;
    }

    // ========================================================================
    // Comparison
    // ========================================================================

    /// Compares two `JsonNumber` values for equality.
    ///
    /// Numbers of different kinds are compared as doubles.
    [[nodiscard]] auto operator==(const JsonNumber& other) const -> bool {
        if (kind != other.kind) {
            return as_f64() == other.as_f64();
        }
        switch (kind) {
        case Kind::Int64:
            return i64 == other.i64;
        case Kind::Uint64:
            return u64 == other.u64;
        case Kind::Double:
            return f64 == other.f64;
        }
        return false;
    }

    /// Compares two `JsonNumber` values for inequality.
    [[nodiscard]] auto operator!=(const JsonNumber& other) const -> bool {
        return !(*this == other);
    }
};

// ============================================================================
// JsonValue
// ============================================================================

/// JSON value variant type representing any JSON value.
///
/// `JsonValue` can hold any of the six JSON types: null, boolean, number,
/// string, array, or object. It uses value semantics with internal boxing
/// for recursive types (array, object) to avoid infinite struct size.
///
/// # Type Hierarchy
///
/// | JSON Type | C++ Storage | Query Method | Accessor |
/// |-----------|-------------|--------------|----------|
/// | `null` | `std::monostate` | `is_null()` | - |
/// | `true/false` | `bool` | `is_bool()` | `as_bool()` |
/// | number | `JsonNumber` | `is_number()` | `as_number()`, `as_i64()`, `as_f64()` |
/// | string | `std::string` | `is_string()` | `as_string()` |
/// | array | `Box<JsonArray>` | `is_array()` | `as_array()`, `operator[]` |
/// | object | `Box<JsonObject>` | `is_object()` | `as_object()`, `get()` |
///
/// # Memory Layout
///
/// Arrays and objects are stored as boxed (heap-allocated) containers to
/// allow recursive structures. This means:
/// - Small values (null, bool, number, short strings) are stored inline
/// - Arrays and objects have one level of indirection
///
/// # Example
///
/// ```cpp
/// // Construction
/// JsonValue null_val;                        // null
/// JsonValue bool_val(true);                  // boolean
/// JsonValue int_val(42);                     // integer number
/// JsonValue float_val(3.14);                 // float number
/// JsonValue str_val("hello");                // string
/// JsonValue arr_val(JsonArray{});            // empty array
/// JsonValue obj_val(JsonObject{});           // empty object
///
/// // Type queries
/// if (obj_val.is_object()) {
///     const auto& obj = obj_val.as_object();
///     // ... work with object
/// }
///
/// // Object access
/// JsonValue config(JsonObject{{"port", JsonValue(8080)}});
/// if (auto* port = config.get("port")) {
///     int p = port->as_i64();
/// }
///
/// // Array access
/// JsonValue items(JsonArray{JsonValue(1), JsonValue(2), JsonValue(3)});
/// for (size_t i = 0; i < items.size(); ++i) {
///     std::cout << items[i].as_i64() << std::endl;
/// }
/// ```
struct JsonValue {
    /// The null type (empty state).
    using Null = std::monostate;

    /// The variant type holding all possible JSON values.
    using ValueVariant = std::variant<Null,             // null
                                      bool,             // boolean
                                      JsonNumber,       // number
                                      std::string,      // string
                                      Box<JsonArray>,   // array (boxed)
                                      Box<JsonObject>>; // object (boxed)

    /// The underlying variant storage.
    ValueVariant data;

    // ========================================================================
    // Constructors
    // ========================================================================

    /// Default constructor creates a `null` value.
    JsonValue() : data(Null{}) {}

    /// Constructs a `null` value explicitly.
    ///
    /// # Arguments
    ///
    /// * `nullptr` - Explicit null
    explicit JsonValue(std::nullptr_t) : data(Null{}) {}

    /// Constructs a boolean value.
    ///
    /// # Arguments
    ///
    /// * `value` - The boolean value (`true` or `false`)
    explicit JsonValue(bool value) : data(value) {}

    /// Constructs an integer value from `int`.
    ///
    /// This constructor handles both `int` and `int32_t` since they are
    /// typically the same type on most platforms.
    ///
    /// # Arguments
    ///
    /// * `value` - The integer value
    explicit JsonValue(int value) : data(JsonNumber(static_cast<int64_t>(value))) {}

    /// Constructs an integer value from `int64_t`.
    ///
    /// # Arguments
    ///
    /// * `value` - The integer value
    explicit JsonValue(int64_t value) : data(JsonNumber(value)) {}

    /// Constructs an unsigned integer value from `uint64_t`.
    ///
    /// # Arguments
    ///
    /// * `value` - The unsigned integer value
    explicit JsonValue(uint64_t value) : data(JsonNumber(value)) {}

    /// Constructs a floating-point value from `double`.
    ///
    /// # Arguments
    ///
    /// * `value` - The floating-point value
    explicit JsonValue(double value) : data(JsonNumber(value)) {}

    /// Constructs a string value from a C string.
    ///
    /// # Arguments
    ///
    /// * `value` - Null-terminated C string
    explicit JsonValue(const char* value) : data(std::string(value)) {}

    /// Constructs a string value from `std::string`.
    ///
    /// # Arguments
    ///
    /// * `value` - The string value (moved)
    explicit JsonValue(std::string value) : data(std::move(value)) {}

    /// Constructs a string value from `std::string_view`.
    ///
    /// # Arguments
    ///
    /// * `value` - The string view (copied)
    explicit JsonValue(std::string_view value) : data(std::string(value)) {}

    /// Constructs an array value.
    ///
    /// # Arguments
    ///
    /// * `value` - The array (moved)
    explicit JsonValue(JsonArray value) : data(make_box<JsonArray>(std::move(value))) {}

    /// Constructs an object value.
    ///
    /// # Arguments
    ///
    /// * `value` - The object (moved)
    explicit JsonValue(JsonObject value) : data(make_box<JsonObject>(std::move(value))) {}

    /// Constructs a number value from `JsonNumber`.
    ///
    /// # Arguments
    ///
    /// * `value` - The number
    explicit JsonValue(JsonNumber value) : data(value) {}

    // ========================================================================
    // Type Queries
    // ========================================================================

    /// Returns `true` if this value is `null`.
    [[nodiscard]] auto is_null() const -> bool {
        return std::holds_alternative<Null>(data);
    }

    /// Returns `true` if this value is a boolean.
    [[nodiscard]] auto is_bool() const -> bool {
        return std::holds_alternative<bool>(data);
    }

    /// Returns `true` if this value is a number (integer or float).
    [[nodiscard]] auto is_number() const -> bool {
        return std::holds_alternative<JsonNumber>(data);
    }

    /// Returns `true` if this value is a string.
    [[nodiscard]] auto is_string() const -> bool {
        return std::holds_alternative<std::string>(data);
    }

    /// Returns `true` if this value is an array.
    [[nodiscard]] auto is_array() const -> bool {
        return std::holds_alternative<Box<JsonArray>>(data);
    }

    /// Returns `true` if this value is an object.
    [[nodiscard]] auto is_object() const -> bool {
        return std::holds_alternative<Box<JsonObject>>(data);
    }

    /// Returns `true` if this value is an integer number.
    ///
    /// An integer number has no decimal point or exponent in the original JSON.
    [[nodiscard]] auto is_integer() const -> bool {
        if (auto* num = std::get_if<JsonNumber>(&data)) {
            return num->is_integer();
        }
        return false;
    }

    /// Returns `true` if this value is a floating-point number.
    [[nodiscard]] auto is_float() const -> bool {
        if (auto* num = std::get_if<JsonNumber>(&data)) {
            return num->is_float();
        }
        return false;
    }

    // ========================================================================
    // Type Accessors
    // ========================================================================

    /// Gets the boolean value.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not a boolean.
    [[nodiscard]] auto as_bool() const -> bool {
        return std::get<bool>(data);
    }

    /// Gets the number value.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not a number.
    [[nodiscard]] auto as_number() const -> const JsonNumber& {
        return std::get<JsonNumber>(data);
    }

    /// Gets the string value.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not a string.
    [[nodiscard]] auto as_string() const -> const std::string& {
        return std::get<std::string>(data);
    }

    /// Gets the array value.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not an array.
    [[nodiscard]] auto as_array() const -> const JsonArray& {
        return *std::get<Box<JsonArray>>(data);
    }

    /// Gets the object value.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not an object.
    [[nodiscard]] auto as_object() const -> const JsonObject& {
        return *std::get<Box<JsonObject>>(data);
    }

    // ========================================================================
    // Mutable Accessors
    // ========================================================================

    /// Gets a mutable reference to the array.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not an array.
    [[nodiscard]] auto as_array_mut() -> JsonArray& {
        return *std::get<Box<JsonArray>>(data);
    }

    /// Gets a mutable reference to the object.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not an object.
    [[nodiscard]] auto as_object_mut() -> JsonObject& {
        return *std::get<Box<JsonObject>>(data);
    }

    // ========================================================================
    // Number Convenience Accessors
    // ========================================================================

    /// Gets the number as `int64_t`.
    ///
    /// # Panics
    ///
    /// Throws `std::runtime_error` if this is not an integer or would overflow.
    [[nodiscard]] auto as_i64() const -> int64_t {
        auto opt = as_number().try_as_i64();
        if (!opt) {
            throw std::runtime_error("JSON number cannot be converted to int64_t");
        }
        return *opt;
    }

    /// Gets the number as `uint64_t`.
    ///
    /// # Panics
    ///
    /// Throws `std::runtime_error` if this is negative or would overflow.
    [[nodiscard]] auto as_u64() const -> uint64_t {
        auto opt = as_number().try_as_u64();
        if (!opt) {
            throw std::runtime_error("JSON number cannot be converted to uint64_t");
        }
        return *opt;
    }

    /// Gets the number as `double`.
    ///
    /// This always succeeds for numbers but may lose precision for large integers.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not a number.
    [[nodiscard]] auto as_f64() const -> double {
        return as_number().as_f64();
    }

    /// Attempts to get the number as `int64_t`.
    ///
    /// Returns `std::nullopt` if this is not an integer or would overflow.
    [[nodiscard]] auto try_as_i64() const -> std::optional<int64_t> {
        if (auto* num = std::get_if<JsonNumber>(&data)) {
            return num->try_as_i64();
        }
        return std::nullopt;
    }

    /// Attempts to get the number as `uint64_t`.
    ///
    /// Returns `std::nullopt` if this is not a non-negative integer.
    [[nodiscard]] auto try_as_u64() const -> std::optional<uint64_t> {
        if (auto* num = std::get_if<JsonNumber>(&data)) {
            return num->try_as_u64();
        }
        return std::nullopt;
    }

    // ========================================================================
    // Object Access
    // ========================================================================

    /// Gets a value from an object by key.
    ///
    /// Returns `nullptr` if:
    /// - This is not an object
    /// - The key does not exist
    ///
    /// # Arguments
    ///
    /// * `key` - The key to look up
    ///
    /// # Returns
    ///
    /// Pointer to the value, or `nullptr` if not found.
    [[nodiscard]] auto get(const std::string& key) const -> const JsonValue* {
        if (auto* obj = std::get_if<Box<JsonObject>>(&data)) {
            auto it = (*obj)->find(key);
            if (it != (*obj)->end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

    /// Gets a mutable value from an object by key.
    ///
    /// # Arguments
    ///
    /// * `key` - The key to look up
    ///
    /// # Returns
    ///
    /// Pointer to the value, or `nullptr` if not found.
    [[nodiscard]] auto get_mut(const std::string& key) -> JsonValue* {
        if (auto* obj = std::get_if<Box<JsonObject>>(&data)) {
            auto it = (*obj)->find(key);
            if (it != (*obj)->end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

    /// Returns `true` if this object contains the given key.
    ///
    /// Returns `false` if this is not an object.
    [[nodiscard]] auto contains(const std::string& key) const -> bool {
        if (auto* obj = std::get_if<Box<JsonObject>>(&data)) {
            return (*obj)->find(key) != (*obj)->end();
        }
        return false;
    }

    // ========================================================================
    // Array Access
    // ========================================================================

    /// Gets an array element by index.
    ///
    /// # Panics
    ///
    /// Throws if this is not an array or index is out of bounds.
    [[nodiscard]] auto operator[](size_t index) const -> const JsonValue& {
        return as_array().at(index);
    }

    /// Gets a mutable array element by index.
    ///
    /// # Panics
    ///
    /// Throws if this is not an array or index is out of bounds.
    [[nodiscard]] auto operator[](size_t index) -> JsonValue& {
        return as_array_mut().at(index);
    }

    /// Gets the size of an array or object.
    ///
    /// Returns `0` for other types.
    [[nodiscard]] auto size() const -> size_t {
        if (auto* arr = std::get_if<Box<JsonArray>>(&data)) {
            return (*arr)->size();
        }
        if (auto* obj = std::get_if<Box<JsonObject>>(&data)) {
            return (*obj)->size();
        }
        return 0;
    }

    // ========================================================================
    // Mutation
    // ========================================================================

    /// Pushes a value to an array.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not an array.
    void push(JsonValue value) {
        as_array_mut().push_back(std::move(value));
    }

    /// Sets a key-value pair in an object.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not an object.
    void set(const std::string& key, JsonValue value) {
        as_object_mut()[key] = std::move(value);
    }

    /// Removes a key from an object.
    ///
    /// # Returns
    ///
    /// `true` if the key existed and was removed, `false` otherwise.
    ///
    /// # Panics
    ///
    /// Throws `std::bad_variant_access` if this is not an object.
    auto remove(const std::string& key) -> bool {
        return as_object_mut().erase(key) > 0;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    /// Serializes this value to a compact JSON string.
    ///
    /// No whitespace is added between elements.
    ///
    /// # Returns
    ///
    /// A compact JSON string representation.
    [[nodiscard]] auto to_string() const -> std::string;

    /// Serializes this value to a pretty-printed JSON string.
    ///
    /// Arrays and objects are indented with the specified number of spaces.
    ///
    /// # Arguments
    ///
    /// * `indent` - Number of spaces per indentation level (default: 2)
    ///
    /// # Returns
    ///
    /// A formatted JSON string with newlines and indentation.
    [[nodiscard]] auto to_string_pretty(int indent = 2) const -> std::string;

    /// Writes this value to an output stream in compact format.
    ///
    /// This avoids intermediate string allocations for large JSON values.
    ///
    /// # Arguments
    ///
    /// * `os` - The output stream to write to
    ///
    /// # Returns
    ///
    /// Reference to the output stream.
    auto write_to(std::ostream& os) const -> std::ostream&;

    /// Writes this value to an output stream in pretty-printed format.
    ///
    /// # Arguments
    ///
    /// * `os` - The output stream to write to
    /// * `indent` - Number of spaces per indentation level (default: 2)
    ///
    /// # Returns
    ///
    /// Reference to the output stream.
    auto write_to_pretty(std::ostream& os, int indent = 2) const -> std::ostream&;

    /// Estimates the serialized size of this JSON value in bytes.
    ///
    /// This provides a hint for buffer pre-allocation when serializing.
    /// The estimate is typically slightly larger than the actual size to
    /// account for escaping and formatting overhead.
    ///
    /// # Returns
    ///
    /// Estimated size in bytes for the compact JSON representation.
    ///
    /// # Example
    ///
    /// ```cpp
    /// JsonValue obj(JsonObject{{"name", JsonValue("Alice")}, {"age", JsonValue(30)}});
    /// std::string buffer;
    /// buffer.reserve(obj.estimated_size());  // Pre-allocate based on hint
    /// buffer = obj.to_string();
    /// ```
    [[nodiscard]] auto estimated_size() const -> size_t;

    // ========================================================================
    // Merging
    // ========================================================================

    /// Merges another object into this object.
    ///
    /// Keys from `other` are added to this object. If a key exists in both,
    /// the value from `other` replaces the existing value.
    ///
    /// # Arguments
    ///
    /// * `other` - The object to merge from
    ///
    /// # Panics
    ///
    /// Throws if either value is not an object.
    ///
    /// # Example
    ///
    /// ```cpp
    /// JsonValue a(JsonObject{{"x", JsonValue(1)}});
    /// JsonValue b(JsonObject{{"y", JsonValue(2)}});
    /// a.merge(std::move(b));
    /// // a is now {"x": 1, "y": 2}
    /// ```
    void merge(JsonValue other);

    /// Extends this array with elements from another array.
    ///
    /// Elements from `other` are appended to this array.
    ///
    /// # Arguments
    ///
    /// * `other` - The array to extend from
    ///
    /// # Panics
    ///
    /// Throws if either value is not an array.
    ///
    /// # Example
    ///
    /// ```cpp
    /// JsonValue a(JsonArray{JsonValue(1), JsonValue(2)});
    /// JsonValue b(JsonArray{JsonValue(3), JsonValue(4)});
    /// a.extend(std::move(b));
    /// // a is now [1, 2, 3, 4]
    /// ```
    void extend(JsonValue other);

    // ========================================================================
    // Cloning
    // ========================================================================

    /// Creates a deep copy of this value.
    ///
    /// Since `JsonValue` uses `unique_ptr` internally for arrays and objects,
    /// copy construction is disabled. Use this method to create copies.
    ///
    /// # Returns
    ///
    /// A new `JsonValue` that is a deep copy of this value.
    ///
    /// # Example
    ///
    /// ```cpp
    /// JsonValue original(JsonArray{JsonValue(1), JsonValue(2)});
    /// JsonValue copy = original.clone();
    /// // original and copy are independent
    /// ```
    [[nodiscard]] auto clone() const -> JsonValue {
        if (is_null()) {
            return JsonValue();
        }
        if (is_bool()) {
            return JsonValue(as_bool());
        }
        if (is_number()) {
            return JsonValue(as_number());
        }
        if (is_string()) {
            return JsonValue(as_string());
        }
        if (is_array()) {
            JsonArray arr;
            arr.reserve(as_array().size());
            for (const auto& elem : as_array()) {
                arr.push_back(elem.clone());
            }
            return JsonValue(std::move(arr));
        }
        if (is_object()) {
            JsonObject obj;
            for (const auto& [key, val] : as_object()) {
                obj[key] = val.clone();
            }
            return JsonValue(std::move(obj));
        }
        return JsonValue();
    }

    // ========================================================================
    // Comparison
    // ========================================================================

    /// Compares two `JsonValue` values for equality.
    ///
    /// Values of different types are never equal.
    /// Arrays are compared element-by-element.
    /// Objects are compared by key-value pairs.
    [[nodiscard]] auto operator==(const JsonValue& other) const -> bool;

    /// Compares two `JsonValue` values for inequality.
    [[nodiscard]] auto operator!=(const JsonValue& other) const -> bool {
        return !(*this == other);
    }
};

// ============================================================================
// Factory Functions
// ============================================================================

/// Creates a `null` JSON value.
///
/// # Example
///
/// ```cpp
/// auto val = json_null();
/// assert(val.is_null());
/// ```
inline auto json_null() -> JsonValue {
    return JsonValue();
}

/// Creates a boolean JSON value.
///
/// # Arguments
///
/// * `value` - The boolean value
///
/// # Example
///
/// ```cpp
/// auto val = json_bool(true);
/// assert(val.as_bool() == true);
/// ```
inline auto json_bool(bool value) -> JsonValue {
    return JsonValue(value);
}

/// Creates an integer JSON value.
///
/// # Arguments
///
/// * `value` - The integer value
///
/// # Example
///
/// ```cpp
/// auto val = json_int(42);
/// assert(val.is_integer());
/// assert(val.as_i64() == 42);
/// ```
inline auto json_int(int64_t value) -> JsonValue {
    return JsonValue(value);
}

/// Creates an unsigned integer JSON value.
///
/// # Arguments
///
/// * `value` - The unsigned integer value
inline auto json_uint(uint64_t value) -> JsonValue {
    return JsonValue(value);
}

/// Creates a floating-point JSON value.
///
/// # Arguments
///
/// * `value` - The floating-point value
///
/// # Example
///
/// ```cpp
/// auto val = json_float(3.14);
/// assert(val.is_float());
/// ```
inline auto json_float(double value) -> JsonValue {
    return JsonValue(value);
}

/// Creates a string JSON value.
///
/// # Arguments
///
/// * `value` - The string value
///
/// # Example
///
/// ```cpp
/// auto val = json_string("hello");
/// assert(val.as_string() == "hello");
/// ```
inline auto json_string(std::string value) -> JsonValue {
    return JsonValue(std::move(value));
}

/// Creates an empty array JSON value.
///
/// # Example
///
/// ```cpp
/// auto arr = json_array();
/// arr.push(json_int(1));
/// arr.push(json_int(2));
/// ```
inline auto json_array() -> JsonValue {
    return JsonValue(JsonArray{});
}

/// Creates an empty object JSON value.
///
/// # Example
///
/// ```cpp
/// auto obj = json_object();
/// obj.set("name", json_string("Alice"));
/// ```
inline auto json_object() -> JsonValue {
    return JsonValue(JsonObject{});
}

} // namespace tml::json
