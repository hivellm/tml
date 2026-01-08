//! # Module Metadata Serialization
//!
//! This module handles serialization and deserialization of TML module metadata.
//! Module metadata (`.tml.meta` files) enables incremental compilation by caching
//! type information for compiled modules.
//!
//! ## File Format
//!
//! Module metadata is stored as JSON containing:
//! - Function signatures
//! - Struct and enum definitions
//! - Behavior declarations
//! - Type aliases
//! - Re-exports
//!
//! ## Path Conventions
//!
//! - Source: `lib/std/src/math.tml`
//! - Metadata: `packages/std/compiled/math.tml.meta`
//! - Object: `packages/std/compiled/math.o`

#ifndef TML_TYPES_MODULE_METADATA_HPP
#define TML_TYPES_MODULE_METADATA_HPP

#include "types/module.hpp"

#include <filesystem>
#include <string>

namespace tml::types {

/// Handles module metadata serialization for incremental compilation.
///
/// Module metadata caches type information in JSON format, allowing
/// the compiler to avoid re-parsing unchanged modules.
class ModuleMetadata {
public:
    /// Serializes a module to JSON format.
    static auto serialize(const Module& module) -> std::string;

    /// Deserializes JSON content to a Module.
    static auto deserialize(const std::string& json_content) -> std::optional<Module>;

    /// Loads module metadata from a `.tml.meta` file.
    static auto load_from_file(const std::filesystem::path& meta_file) -> std::optional<Module>;

    /// Saves module metadata to a `.tml.meta` file.
    static bool save_to_file(const Module& module, const std::filesystem::path& meta_file);

    /// Returns the metadata file path for a module.
    ///
    /// Example: `"std::math"` -> `"packages/std/compiled/math.tml.meta"`
    static auto get_metadata_path(const std::string& module_path) -> std::filesystem::path;

    /// Returns the object file path for a module.
    ///
    /// Example: `"std::math"` -> `"packages/std/compiled/math.o"`
    static auto get_object_path(const std::string& module_path) -> std::filesystem::path;

    /// Returns true if compiled metadata exists for the module.
    static bool has_compiled_metadata(const std::string& module_path);
};

} // namespace tml::types

#endif // TML_TYPES_MODULE_METADATA_HPP
