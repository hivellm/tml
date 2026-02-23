TML_MODULE("compiler")

//! # HIR Serialization Entry Point
//!
//! This file provides serialization and deserialization of HIR modules.
//! The implementation is split into modular components in the serializer/ directory.
//!
//! ## Module Structure
//!
//! | File                        | Contents                  |
//! |-----------------------------|---------------------------|
//! | `serializer_internal.hpp`   | Common types and tags     |
//! | `binary_writer.cpp`         | Binary format writer      |
//! | `binary_reader.cpp`         | Binary format reader      |
//! | `text_writer.cpp`           | Text format writer        |
//! | `text_reader.cpp`           | Text format reader        |
//! | `serialize_utils.cpp`       | Convenience functions     |
//!
//! ## Usage
//!
//! ```cpp
//! // Serialize to binary
//! auto bytes = serialize_hir_binary(module);
//!
//! // Deserialize from binary
//! auto module = deserialize_hir_binary(bytes);
//! ```

#include "hir/hir_serialize.hpp"

// All implementation is in the serializer/ subdirectory
