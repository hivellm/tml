//! # JSON Error Types
//!
//! This module provides error types for JSON parsing and validation operations.
//! Errors include precise source location information for diagnostics.
//!
//! ## Features
//!
//! - **Location tracking**: Line, column, and byte offset information
//! - **Human-readable messages**: Formatted error strings for display
//! - **Factory methods**: Convenient error construction
//!
//! ## Example
//!
//! ```cpp
//! // Create error with location
//! auto error = JsonError::make("Unexpected token", 5, 12);
//! std::cerr << error.to_string() << std::endl;
//! // Output: "line 5, column 12: Unexpected token"
//!
//! // Create error without location
//! auto simple_error = JsonError::make("Invalid JSON");
//! std::cerr << simple_error.to_string() << std::endl;
//! // Output: "Invalid JSON"
//! ```

#pragma once

#include <cstddef>
#include <string>

namespace tml::json {

/// An error encountered during JSON parsing or validation.
///
/// `JsonError` contains a human-readable message and optional source location
/// information. The location fields help pinpoint exactly where in the input
/// the error occurred.
///
/// # Fields
///
/// - `message`: Description of what went wrong
/// - `line`: 1-based line number (0 if unknown)
/// - `column`: 1-based column number (0 if unknown)
/// - `offset`: Byte offset from start of input (0 if unknown)
///
/// # Example
///
/// ```cpp
/// auto error = JsonError::make("Unterminated string", 10, 5, 156);
/// if (error.line > 0) {
///     std::cerr << "Error at line " << error.line << std::endl;
/// }
/// ```
struct JsonError {
    /// Human-readable error description.
    std::string message;

    /// Line number where the error occurred (1-based, 0 if unknown).
    size_t line = 0;

    /// Column number where the error occurred (1-based, 0 if unknown).
    size_t column = 0;

    /// Byte offset in input where the error occurred (0 if unknown).
    size_t offset = 0;

    /// Creates an error with message only.
    ///
    /// Use this when location information is not available.
    ///
    /// # Arguments
    ///
    /// * `msg` - The error message
    ///
    /// # Returns
    ///
    /// A `JsonError` with the message and zero location fields.
    static auto make(std::string msg) -> JsonError {
        return JsonError{std::move(msg), 0, 0, 0};
    }

    /// Creates an error with full location information.
    ///
    /// # Arguments
    ///
    /// * `msg` - The error message
    /// * `line` - 1-based line number
    /// * `column` - 1-based column number
    /// * `offset` - Byte offset (optional, defaults to 0)
    ///
    /// # Returns
    ///
    /// A `JsonError` with message and location.
    static auto make(std::string msg, size_t line, size_t column, size_t offset = 0) -> JsonError {
        return JsonError{std::move(msg), line, column, offset};
    }

    /// Formats the error as a human-readable string.
    ///
    /// The format depends on available location information:
    /// - With line and column: `"line X, column Y: message"`
    /// - With line only: `"line X: message"`
    /// - Without location: `"message"`
    ///
    /// # Returns
    ///
    /// A formatted error string suitable for display.
    [[nodiscard]] auto to_string() const -> std::string {
        if (line > 0 && column > 0) {
            return "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " +
                   message;
        }
        if (line > 0) {
            return "line " + std::to_string(line) + ": " + message;
        }
        return message;
    }
};

} // namespace tml::json
