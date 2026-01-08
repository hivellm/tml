//! # MIR Text Writer
//!
//! This file writes MIR modules to human-readable text format.
//!
//! ## Implementation
//!
//! Delegates to `MirPrinter` for actual formatting. Supports:
//! - Compact mode: Minimal whitespace
//! - Pretty mode: Indented, readable output
//!
//! ## Output Format
//!
//! ```text
//! ; MIR Module: name
//!
//! struct Point { x: I32, y: I32 }
//!
//! func @add(%a: I32, %b: I32) -> I32 {
//! bb0:
//!     %0 = add %a, %b
//!     ret %0
//! }
//! ```
//!
//! ## Use Cases
//!
//! - Debugging MIR output
//! - `--emit-mir` flag output
//! - Testing and verification

#include "serializer_internal.hpp"

namespace tml::mir {

// ============================================================================
// MirTextWriter Implementation
// ============================================================================

MirTextWriter::MirTextWriter(std::ostream& out, SerializeOptions options)
    : out_(out), options_(options) {}

void MirTextWriter::write_module(const Module& module) {
    MirPrinter printer(!options_.compact);
    out_ << printer.print_module(module);
}

} // namespace tml::mir
