//! # JSON Value Implementation
//!
//! This module implements the equality comparison operator for `JsonValue`.
//! Most `JsonValue` methods are implemented inline in the header file, and
//! serialization methods are in `json_serializer.cpp`.
//!
//! ## Equality Semantics
//!
//! JSON value equality follows these rules:
//!
//! | Type | Comparison Rule |
//! |------|-----------------|
//! | `null` | All nulls are equal |
//! | `bool` | Standard boolean comparison |
//! | `number` | Numeric comparison (see `JsonNumber::operator==`) |
//! | `string` | Byte-by-byte string comparison |
//! | `array` | Element-by-element in order |
//! | `object` | Key-value pair comparison (order independent) |
//!
//! Values of different types are never equal, even if they could be
//! semantically equivalent (e.g., `1` and `1.0` are not equal if stored
//! differently).
//!
//! ## Example
//!
//! ```cpp
//! JsonValue a(42);
//! JsonValue b(42);
//! JsonValue c(42.0);
//!
//! assert(a == b);      // Same type and value
//! assert(!(a == c));   // Different storage (int vs double)
//!
//! JsonValue arr1(JsonArray{JsonValue(1), JsonValue(2)});
//! JsonValue arr2(JsonArray{JsonValue(1), JsonValue(2)});
//! assert(arr1 == arr2);  // Arrays with same elements
//! ```

#include "json/json_value.hpp"

namespace tml::json {

/// Compares two `JsonValue` instances for equality.
///
/// Values of different types are never equal. For composite types:
/// - Arrays are compared element-by-element in order
/// - Objects are compared by key-value pairs (order independent)
///
/// # Arguments
///
/// * `other` - The value to compare against
///
/// # Returns
///
/// `true` if both values have the same type and equal content,
/// `false` otherwise.
///
/// # Example
///
/// ```cpp
/// JsonValue a(JsonObject{{"key", JsonValue("value")}});
/// JsonValue b(JsonObject{{"key", JsonValue("value")}});
/// assert(a == b);
/// ```
auto JsonValue::operator==(const JsonValue& other) const -> bool {
    // Different variant types are not equal
    if (data.index() != other.data.index()) {
        return false;
    }

    // Compare based on type
    if (is_null()) {
        return true; // Both are null
    }

    if (is_bool()) {
        return as_bool() == other.as_bool();
    }

    if (is_number()) {
        return as_number() == other.as_number();
    }

    if (is_string()) {
        return as_string() == other.as_string();
    }

    if (is_array()) {
        const auto& arr1 = as_array();
        const auto& arr2 = other.as_array();
        if (arr1.size() != arr2.size()) {
            return false;
        }
        for (size_t i = 0; i < arr1.size(); ++i) {
            if (arr1[i] != arr2[i]) {
                return false;
            }
        }
        return true;
    }

    if (is_object()) {
        const auto& obj1 = as_object();
        const auto& obj2 = other.as_object();
        if (obj1.size() != obj2.size()) {
            return false;
        }
        for (const auto& [key, val] : obj1) {
            auto it = obj2.find(key);
            if (it == obj2.end() || it->second != val) {
                return false;
            }
        }
        return true;
    }

    return false;
}

/// Merges another object into this object.
///
/// All key-value pairs from `other` are inserted or updated in this object.
/// If a key already exists, its value is replaced.
///
/// # Arguments
///
/// * `other` - The object to merge from (moved)
///
/// # Panics
///
/// Throws `std::bad_variant_access` if either value is not an object.
void JsonValue::merge(JsonValue other) {
    auto& this_obj = as_object_mut();
    auto& other_obj = other.as_object_mut();

    for (auto& [key, val] : other_obj) {
        this_obj[key] = std::move(val);
    }
}

/// Extends this array with elements from another array.
///
/// All elements from `other` are appended to this array.
///
/// # Arguments
///
/// * `other` - The array to extend from (moved)
///
/// # Panics
///
/// Throws `std::bad_variant_access` if either value is not an array.
void JsonValue::extend(JsonValue other) {
    auto& this_arr = as_array_mut();
    auto& other_arr = other.as_array_mut();

    this_arr.reserve(this_arr.size() + other_arr.size());
    for (auto& elem : other_arr) {
        this_arr.push_back(std::move(elem));
    }
}

} // namespace tml::json
