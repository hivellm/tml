//! # TypeEnv Binary Deserializer
//!
//! Reads the TypeEnv binary format produced by `lib/std/src/serial/typeenv.tml`
//! and reconstructs an in-memory `types::TypeEnv`.
//!
//! ## Format
//!
//! The format is documented in `lib/std/src/serial/typeenv.tml`. Key points:
//!
//! - Magic: u32 LE `0x54454E56` ("TENV")
//! - Version: major u8 = 1, minor u8 (forward compatible within major 1)
//! - Strings: varint length + raw UTF-8 bytes
//! - Counts: LEB-128 unsigned varints
//! - Semantic types: inline binary encoding with u8 tag per type variant
//! - Sections: structs, enums, behaviors, functions, behavior_impls,
//!   type_aliases, classes, interfaces, class_interfaces, builtins

#ifndef TML_SERIAL_TYPEENV_READER_HPP
#define TML_SERIAL_TYPEENV_READER_HPP

#include "types/env.hpp"

#include <cstdint>
#include <vector>

namespace tml::serial {

/// Deserializes a TypeEnv binary blob into a `types::TypeEnv`.
///
/// The returned TypeEnv contains all type definitions (structs, enums,
/// behaviors, functions, classes, interfaces) and metadata (behavior impls,
/// type aliases, builtins). Transient state (scopes, type vars, imports)
/// is not serialized and starts in default state.
///
/// Throws `std::runtime_error` on:
/// - Magic mismatch
/// - Major version mismatch (only major == 1 is supported)
/// - Unknown semantic type tag
/// - Premature end-of-input
types::TypeEnv read_typeenv(const std::vector<uint8_t>& data);

} // namespace tml::serial

#endif // TML_SERIAL_TYPEENV_READER_HPP
