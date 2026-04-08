//! # AST Binary Deserializer
//!
//! Reads the AST binary format produced by `lib/std/src/serial/ast.tml`
//! and reconstructs an in-memory `parser::Module`.
//!
//! ## Format
//!
//! The format is documented in `lib/std/src/serial/ast.tml`. Key points:
//!
//! - Magic: u32 LE `0x41535420` ("AST ")
//! - Version: major u8 = 1, minor u8 (forward compatible within major 1)
//! - Strings: varint length + raw UTF-8 bytes
//! - Counts: LEB-128 unsigned varints
//! - Integers: little-endian (u32 = 4 bytes, u64 = 8 bytes)
//! - SourceLocation: 4 × u32 LE (line, column, offset, length); the `file`
//!   field is NOT serialized — it is injected from the `file_path` argument
//!   passed to `read_ast()` for every reconstructed location.
//!
//! ## Lifetime
//!
//! `SourceLocation::file` is a `std::string_view`. The caller MUST keep the
//! string referenced by `file_path` alive for as long as the returned
//! `parser::Module` (or any of its child nodes) is used.

#ifndef TML_SERIAL_AST_READER_HPP
#define TML_SERIAL_AST_READER_HPP

#include "parser/ast.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace tml::serial {

/// Deserializes an AST binary blob into a `parser::Module`.
///
/// Throws `std::runtime_error` on:
/// - Magic mismatch
/// - Major version mismatch (only major == 1 is supported)
/// - Unknown variant tag
/// - Premature end-of-input
/// - Invalid UTF-8 length
///
/// `file_path` is injected into every reconstructed `SourceLocation::file`.
/// The string referenced by `file_path` must outlive the returned module.
parser::Module read_ast(const std::vector<uint8_t>& data, std::string_view file_path);

} // namespace tml::serial

#endif // TML_SERIAL_AST_READER_HPP
