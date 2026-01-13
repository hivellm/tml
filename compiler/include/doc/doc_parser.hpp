//! # Documentation Comment Parser
//!
//! This module parses documentation comment text to extract structured
//! information like parameters, return values, examples, and cross-references.
//!
//! ## Supported Tags
//!
//! - `@param name description` - Document a parameter
//! - `@returns description` - Document the return value
//! - `@throws ErrorType description` - Document an error condition
//! - `@example` - Start a code example block
//! - `@see symbol` - Cross-reference another item
//! - `@since version` - Mark when item was introduced
//! - `@deprecated message` - Mark item as deprecated
//!
//! ## Code Blocks
//!
//! Fenced code blocks are automatically extracted as examples:
//! ```tml
//! let x = 42
//! ```

#ifndef TML_DOC_DOC_PARSER_HPP
#define TML_DOC_DOC_PARSER_HPP

#include "doc/doc_model.hpp"

#include <string>
#include <vector>

namespace tml::doc {

/// Result of parsing a documentation comment.
struct ParsedDoc {
    std::string summary;                      ///< First paragraph (before any tags).
    std::string body;                         ///< Full body text (markdown).
    std::vector<DocParam> params;             ///< @param tags.
    std::optional<DocReturn> returns;         ///< @returns tag.
    std::vector<DocThrows> throws;            ///< @throws tags.
    std::vector<DocExample> examples;         ///< @example tags and code blocks.
    std::vector<std::string> see_also;        ///< @see references.
    std::optional<std::string> since;         ///< @since version.
    std::optional<DocDeprecation> deprecated; ///< @deprecated info.
};

/// Parses a documentation comment string.
///
/// Extracts structured information from the markdown-formatted doc comment.
///
/// @param doc_text The raw documentation text.
/// @returns Parsed documentation with extracted tags.
[[nodiscard]] auto parse_doc_comment(const std::string& doc_text) -> ParsedDoc;

/// Extracts the first paragraph from a documentation string.
///
/// The summary is the text before the first blank line or tag.
///
/// @param doc_text The raw documentation text.
/// @returns The first paragraph as summary.
[[nodiscard]] auto extract_summary(const std::string& doc_text) -> std::string;

/// Extracts code blocks from documentation text.
///
/// Finds fenced code blocks (```tml ... ```) and returns them as examples.
///
/// @param doc_text The documentation text.
/// @returns Vector of extracted code examples.
[[nodiscard]] auto extract_code_blocks(const std::string& doc_text) -> std::vector<DocExample>;

} // namespace tml::doc

#endif // TML_DOC_DOC_PARSER_HPP
