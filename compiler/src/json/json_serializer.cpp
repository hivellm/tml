//! # JSON Serializer
//!
//! This module implements JSON serialization for `JsonValue`.
//! It converts JSON values to string representation in either compact
//! or pretty-printed format.
//!
//! ## Features
//!
//! - **Compact output**: No whitespace between elements
//! - **Pretty printing**: Configurable indentation with newlines
//! - **String escaping**: Handles special characters and control codes
//! - **Integer preservation**: Integers are serialized without decimal point
//!
//! ## String Escaping
//!
//! The following characters are escaped in output:
//!
//! | Character | Escape Sequence |
//! |-----------|-----------------|
//! | `"` | `\"` |
//! | `\` | `\\` |
//! | Backspace | `\b` |
//! | Form feed | `\f` |
//! | Line feed | `\n` |
//! | Carriage return | `\r` |
//! | Tab | `\t` |
//! | Control (0x00-0x1F) | `\uXXXX` |
//!
//! ## Example
//!
//! ```cpp
//! JsonValue obj(JsonObject{{"name", JsonValue("Alice")}, {"age", JsonValue(30)}});
//!
//! // Compact output
//! std::string compact = obj.to_string();
//! // {"age":30,"name":"Alice"}
//!
//! // Pretty-printed output
//! std::string pretty = obj.to_string_pretty(2);
//! // {
//! //   "age": 30,
//! //   "name": "Alice"
//! // }
//! ```

#include "json/json_value.hpp"

#include <cmath>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace tml::json {

namespace {

/// Escapes a string for JSON output.
///
/// Handles special characters according to RFC 8259:
/// - `"` becomes `\"`
/// - `\` becomes `\\`
/// - Control characters (0x00-0x1F) become `\uXXXX` or named escapes
///
/// # Arguments
///
/// * `s` - The string to escape
///
/// # Returns
///
/// The escaped string (without surrounding quotes).
auto escape_string(const std::string& s) -> std::string {
    std::string result;
    result.reserve(s.size() + 2);

    for (char c : s) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                // Control character - escape as \u00XX
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                    << static_cast<int>(static_cast<unsigned char>(c));
                result += oss.str();
            } else {
                result += c;
            }
            break;
        }
    }

    return result;
}

/// Formats a JSON number for output.
///
/// Integers are formatted without decimal point to preserve their type.
/// Floats use the shortest representation that round-trips correctly.
/// Special values (NaN, Infinity) are converted to `null`.
///
/// # Arguments
///
/// * `num` - The number to format
///
/// # Returns
///
/// The formatted number string.
auto format_number(const JsonNumber& num) -> std::string {
    switch (num.kind) {
    case JsonNumber::Kind::Int64:
        return std::to_string(num.i64);
    case JsonNumber::Kind::Uint64:
        return std::to_string(num.u64);
    case JsonNumber::Kind::Double: {
        // Handle special cases (JSON doesn't support these)
        if (std::isnan(num.f64)) {
            return "null";
        }
        if (std::isinf(num.f64)) {
            return "null";
        }

        std::ostringstream oss;
        oss << std::setprecision(17) << num.f64;
        std::string result = oss.str();

        // Ensure there's a decimal point for floats
        if (result.find('.') == std::string::npos &&
            result.find('e') == std::string::npos &&
            result.find('E') == std::string::npos) {
            result += ".0";
        }

        return result;
    }
    }
    return "0";
}

/// Serializes a `JsonValue` to a compact JSON string.
///
/// This is a recursive helper function that appends to an output string.
///
/// # Arguments
///
/// * `value` - The value to serialize
/// * `out` - The output string to append to
void serialize_compact(const JsonValue& value, std::string& out) {
    if (value.is_null()) {
        out += "null";
        return;
    }

    if (value.is_bool()) {
        out += value.as_bool() ? "true" : "false";
        return;
    }

    if (value.is_number()) {
        out += format_number(value.as_number());
        return;
    }

    if (value.is_string()) {
        out += '"';
        out += escape_string(value.as_string());
        out += '"';
        return;
    }

    if (value.is_array()) {
        out += '[';
        const auto& arr = value.as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            serialize_compact(arr[i], out);
        }
        out += ']';
        return;
    }

    if (value.is_object()) {
        out += '{';
        const auto& obj = value.as_object();
        bool first = true;
        for (const auto& [key, val] : obj) {
            if (!first) {
                out += ',';
            }
            first = false;
            out += '"';
            out += escape_string(key);
            out += "\":";
            serialize_compact(val, out);
        }
        out += '}';
        return;
    }
}

/// Serializes a `JsonValue` to a pretty-printed JSON string.
///
/// This is a recursive helper function that appends to an output string
/// with proper indentation.
///
/// # Arguments
///
/// * `value` - The value to serialize
/// * `out` - The output string to append to
/// * `indent` - Number of spaces per indentation level
/// * `depth` - Current nesting depth
void serialize_pretty(const JsonValue& value, std::string& out, int indent, int depth) {
    std::string indent_str(static_cast<size_t>(depth * indent), ' ');
    std::string next_indent(static_cast<size_t>((depth + 1) * indent), ' ');

    if (value.is_null()) {
        out += "null";
        return;
    }

    if (value.is_bool()) {
        out += value.as_bool() ? "true" : "false";
        return;
    }

    if (value.is_number()) {
        out += format_number(value.as_number());
        return;
    }

    if (value.is_string()) {
        out += '"';
        out += escape_string(value.as_string());
        out += '"';
        return;
    }

    if (value.is_array()) {
        const auto& arr = value.as_array();
        if (arr.empty()) {
            out += "[]";
            return;
        }

        out += "[\n";
        for (size_t i = 0; i < arr.size(); ++i) {
            out += next_indent;
            serialize_pretty(arr[i], out, indent, depth + 1);
            if (i + 1 < arr.size()) {
                out += ',';
            }
            out += '\n';
        }
        out += indent_str;
        out += ']';
        return;
    }

    if (value.is_object()) {
        const auto& obj = value.as_object();
        if (obj.empty()) {
            out += "{}";
            return;
        }

        out += "{\n";
        size_t i = 0;
        for (const auto& [key, val] : obj) {
            out += next_indent;
            out += '"';
            out += escape_string(key);
            out += "\": ";
            serialize_pretty(val, out, indent, depth + 1);
            if (i + 1 < obj.size()) {
                out += ',';
            }
            out += '\n';
            ++i;
        }
        out += indent_str;
        out += '}';
        return;
    }
}

/// Writes an escaped string to an output stream.
///
/// # Arguments
///
/// * `os` - The output stream
/// * `s` - The string to escape and write
void write_escaped_string(std::ostream& os, const std::string& s) {
    for (char c : s) {
        switch (c) {
        case '"':
            os << "\\\"";
            break;
        case '\\':
            os << "\\\\";
            break;
        case '\b':
            os << "\\b";
            break;
        case '\f':
            os << "\\f";
            break;
        case '\n':
            os << "\\n";
            break;
        case '\r':
            os << "\\r";
            break;
        case '\t':
            os << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                os << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                   << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
            } else {
                os << c;
            }
            break;
        }
    }
}

/// Writes a JSON number to an output stream.
///
/// # Arguments
///
/// * `os` - The output stream
/// * `num` - The number to write
void write_number(std::ostream& os, const JsonNumber& num) {
    switch (num.kind) {
    case JsonNumber::Kind::Int64:
        os << num.i64;
        break;
    case JsonNumber::Kind::Uint64:
        os << num.u64;
        break;
    case JsonNumber::Kind::Double:
        if (std::isnan(num.f64) || std::isinf(num.f64)) {
            os << "null";
        } else {
            os << std::setprecision(17) << num.f64;
        }
        break;
    }
}

/// Writes a JsonValue to an output stream in compact format.
///
/// # Arguments
///
/// * `value` - The value to write
/// * `os` - The output stream
void stream_compact(const JsonValue& value, std::ostream& os) {
    if (value.is_null()) {
        os << "null";
        return;
    }

    if (value.is_bool()) {
        os << (value.as_bool() ? "true" : "false");
        return;
    }

    if (value.is_number()) {
        write_number(os, value.as_number());
        return;
    }

    if (value.is_string()) {
        os << '"';
        write_escaped_string(os, value.as_string());
        os << '"';
        return;
    }

    if (value.is_array()) {
        os << '[';
        const auto& arr = value.as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                os << ',';
            }
            stream_compact(arr[i], os);
        }
        os << ']';
        return;
    }

    if (value.is_object()) {
        os << '{';
        const auto& obj = value.as_object();
        bool first = true;
        for (const auto& [key, val] : obj) {
            if (!first) {
                os << ',';
            }
            first = false;
            os << '"';
            write_escaped_string(os, key);
            os << "\":";
            stream_compact(val, os);
        }
        os << '}';
        return;
    }
}

/// Writes a JsonValue to an output stream in pretty format.
///
/// # Arguments
///
/// * `value` - The value to write
/// * `os` - The output stream
/// * `indent` - Spaces per indentation level
/// * `depth` - Current nesting depth
void stream_pretty(const JsonValue& value, std::ostream& os, int indent, int depth) {
    std::string indent_str(static_cast<size_t>(depth * indent), ' ');
    std::string next_indent(static_cast<size_t>((depth + 1) * indent), ' ');

    if (value.is_null()) {
        os << "null";
        return;
    }

    if (value.is_bool()) {
        os << (value.as_bool() ? "true" : "false");
        return;
    }

    if (value.is_number()) {
        write_number(os, value.as_number());
        return;
    }

    if (value.is_string()) {
        os << '"';
        write_escaped_string(os, value.as_string());
        os << '"';
        return;
    }

    if (value.is_array()) {
        const auto& arr = value.as_array();
        if (arr.empty()) {
            os << "[]";
            return;
        }

        os << "[\n";
        for (size_t i = 0; i < arr.size(); ++i) {
            os << next_indent;
            stream_pretty(arr[i], os, indent, depth + 1);
            if (i + 1 < arr.size()) {
                os << ',';
            }
            os << '\n';
        }
        os << indent_str << ']';
        return;
    }

    if (value.is_object()) {
        const auto& obj = value.as_object();
        if (obj.empty()) {
            os << "{}";
            return;
        }

        os << "{\n";
        size_t i = 0;
        for (const auto& [key, val] : obj) {
            os << next_indent << '"';
            write_escaped_string(os, key);
            os << "\": ";
            stream_pretty(val, os, indent, depth + 1);
            if (i + 1 < obj.size()) {
                os << ',';
            }
            os << '\n';
            ++i;
        }
        os << indent_str << '}';
        return;
    }
}

} // anonymous namespace

// ============================================================================
// Public API Implementation
// ============================================================================

auto JsonValue::to_string() const -> std::string {
    std::string result;
    serialize_compact(*this, result);
    return result;
}

auto JsonValue::to_string_pretty(int indent) const -> std::string {
    std::string result;
    serialize_pretty(*this, result, indent, 0);
    return result;
}

auto JsonValue::write_to(std::ostream& os) const -> std::ostream& {
    stream_compact(*this, os);
    return os;
}

auto JsonValue::write_to_pretty(std::ostream& os, int indent) const -> std::ostream& {
    stream_pretty(*this, os, indent, 0);
    return os;
}

} // namespace tml::json
