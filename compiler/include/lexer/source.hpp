//! # Source File Management
//!
//! This module provides source file representation and utilities for the TML
//! lexer. It handles loading source files, tracking line/column positions,
//! and providing efficient access to source text.
//!
//! ## Features
//!
//! - **UTF-8 support**: Source files are assumed to be UTF-8 encoded
//! - **Line tracking**: Efficient line/column lookup from byte offsets
//! - **Slicing**: Extract substrings for lexeme creation
//! - **Error reporting**: Provides line content for diagnostic messages
//!
//! ## Example
//!
//! ```cpp
//! // Load from file
//! auto result = Source::from_file("program.tml");
//! if (result.is_err()) {
//!     std::cerr << "Failed to load: " << result.error() << "\n";
//!     return;
//! }
//! Source source = std::move(result.value());
//!
//! // Or create from string
//! Source source = Source::from_string("let x = 42", "<test>");
//!
//! // Get location info
//! SourceLocation loc = source.location(4); // line 1, column 5
//! std::string_view line = source.line(1);  // "let x = 42"
//! ```

#ifndef TML_LEXER_SOURCE_HPP
#define TML_LEXER_SOURCE_HPP

#include "common.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tml::lexer {

/// Represents a TML source file with efficient location tracking.
///
/// The `Source` class manages the content of a source file and provides
/// efficient conversion between byte offsets and line/column positions.
/// This is essential for error reporting and source span creation.
///
/// # Line Index
///
/// Upon construction, the source builds an index of line start offsets,
/// enabling O(log n) lookup of line numbers from byte offsets.
///
/// # Memory Model
///
/// The source owns its content string. String views returned by methods
/// like `content()`, `slice()`, and `line()` are valid as long as the
/// Source object exists.
class Source {
public:
    /// Constructs a source from a filename and content.
    ///
    /// Builds the line index automatically.
    Source(std::string filename, std::string content);

    /// Returns the entire source content as a string view.
    [[nodiscard]] auto content() const -> std::string_view {
        return content_;
    }

    /// Returns the filename or identifier for this source.
    [[nodiscard]] auto filename() const -> std::string_view {
        return filename_;
    }

    /// Returns the length of the source in bytes.
    [[nodiscard]] auto length() const -> size_t {
        return content_.size();
    }

    /// Returns the character (byte) at the given offset.
    ///
    /// Returns '\0' if offset is out of bounds.
    [[nodiscard]] auto at(size_t offset) const -> char;

    /// Returns a substring from `start` to `end` (exclusive).
    ///
    /// The range is clamped to valid bounds.
    [[nodiscard]] auto slice(size_t start, size_t end) const -> std::string_view;

    /// Converts a byte offset to a line/column location.
    ///
    /// Uses binary search on the line index for O(log n) lookup.
    /// Line and column numbers are 1-indexed.
    [[nodiscard]] auto location(size_t offset) const -> SourceLocation;

    /// Returns the content of a specific line (1-indexed).
    ///
    /// Returns an empty view if the line number is out of range.
    /// The returned view does not include the trailing newline.
    [[nodiscard]] auto line(uint32_t line_num) const -> std::string_view;

    /// Returns the total number of lines in the source.
    [[nodiscard]] auto line_count() const -> uint32_t;

    /// Loads a source file from disk.
    ///
    /// Returns an error string if the file cannot be read.
    [[nodiscard]] static auto from_file(const std::string& path) -> Result<Source, std::string>;

    /// Creates a source from an in-memory string.
    ///
    /// Useful for tests, REPL, and embedded snippets.
    /// The `name` parameter is used for error messages.
    [[nodiscard]] static auto from_string(std::string content, std::string name = "<input>")
        -> Source;

private:
    std::string filename_;             ///< Filename or identifier.
    std::string content_;              ///< UTF-8 encoded source content.
    std::vector<size_t> line_offsets_; ///< Byte offset of each line start.

    /// Builds the line offset index from content.
    void build_line_index();
};

} // namespace tml::lexer

#endif // TML_LEXER_SOURCE_HPP
